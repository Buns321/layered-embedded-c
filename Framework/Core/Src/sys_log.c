/**
 * @file sys_log.c
 * @brief 非阻塞 System 日志门面实现。
 * @details 属于 Layer 2（Framework/Core），通过短临界区保护门面状态与 try-lock 标志，
 *          在栈上完成格式化（上限 SYS_LOG_LINE_CAPACITY）后非阻塞提交给后端，避免持有
 *          全局锁或关中断期间执行耗时的 vsnprintf 与后端拷贝。
 * @ingroup Framework_Core
 */

#include "sys_log.h"
#include "bsp_mcu.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifndef SYS_LOG_GET_TIME_MS
#if defined(SYS_LOG_EXTERNAL_TIME_SOURCE)
/** @brief 外部注入的毫秒时间戳获取函数（用于宿主机单元测试）。 */
extern uint32_t SYS_LOG_EXTERNAL_TIME_SOURCE(void);
/** @brief 获取当前日志时间戳（单位：ms），优先使用外部注入源。 */
#define SYS_LOG_GET_TIME_MS() SYS_LOG_EXTERNAL_TIME_SOURCE()
#else
/** @brief 获取当前日志时间戳（单位：ms），默认使用 HAL_GetTick()。 */
#define SYS_LOG_GET_TIME_MS() HAL_GetTick()
#endif
#endif

/** @brief 单条格式化日志栈缓冲区最大容量（单位：B，含 CRLF 与 NUL）。 */
#define SYS_LOG_LINE_CAPACITY 192U

/** @brief 快速过滤掩码中表示后端就绪的最高位标志。 */
#define SYS_LOG_FAST_READY_MASK_ (1U << 31)

/** @brief 供 Sys_Log_IsEnabled() 无锁快路径读取的日志就绪与等级位掩码。 */
volatile uint32_t g_sys_log_fast_mask;

/** @brief 当前绑定的日志后端指针。 */
static sys_log_base_t *volatile s_log_sink;

/** @brief 当前最低允许输出的日志等级。 */
static sys_log_level_t volatile s_log_level = SYS_LOG_LEVEL_INFO;

/** @brief 门面非阻塞互斥锁标志（true 表示已有任务正在格式化并提交日志）。 */
static volatile bool s_log_locked;

/** @brief 累计被后端拒绝的日志字节数（单位：B）。 */
static volatile uint32_t s_drop_bytes;

/** @brief 累计因 try-lock 竞争被直接丢弃的日志条数（单位：条）。 */
static volatile uint32_t s_contention_count;

/**
 * @brief 保存当前 PRIMASK 并关闭全局可屏蔽中断，形成极短排他临界区。
 * @return 进入前的中断屏蔽状态快照。
 */
static uint32_t sys_log_enterexclusive(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

/**
 * @brief 按快照恢复 PRIMASK 状态以退出排他临界区。
 * @param[in] primask 由 sys_log_enterexclusive() 返回的屏蔽状态快照。
 */
static void sys_log_exitexclusive(uint32_t primask) { __set_PRIMASK(primask); }

/**
 * @brief 将日志等级映射为单字符标签。
 * @param[in] level 日志等级。
 * @return 单字符等级标签字符串（"E"/"W"/"I"/"D"/"?"）。
 */
static const char *sys_log_getleveltag(sys_log_level_t level) {
  switch (level) {
  case SYS_LOG_LEVEL_ERROR:
    return "E";
  case SYS_LOG_LEVEL_WARN:
    return "W";
  case SYS_LOG_LEVEL_INFO:
    return "I";
  case SYS_LOG_LEVEL_DEBUG:
    return "D";
  default:
    return "?";
  }
}

/**
 * @brief 尝试非阻塞获取门面写入锁。
 * @return 是否成功获取锁。
 */
static bool sys_log_trylock(void) {
  bool acquired = false;
  uint32_t primask = sys_log_enterexclusive();
  if (!s_log_locked) {
    s_log_locked = true;
    acquired = true;
  }
  sys_log_exitexclusive(primask);
  return acquired;
}

/**
 * @brief 释放门面写入锁。
 */
static void sys_log_unlock(void) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_locked = false;
  sys_log_exitexclusive(primask);
}

/**
 * @brief 在排他临界区内原子累加统计计数器。
 * @param[in,out] counter 目标计数器指针。
 * @param[in]     amount  增量值。
 */
static void sys_log_addcounter(volatile uint32_t *counter, uint32_t amount) {
  if (amount == 0U) {
    return;
  }
  uint32_t primask = sys_log_enterexclusive();
  *counter += amount;
  sys_log_exitexclusive(primask);
}

/**
 * @brief 在排他临界区内原子读取统计计数器。
 * @param[in] counter 目标计数器指针。
 * @return 当前计数值。
 */
static uint32_t sys_log_readcounter(volatile uint32_t *counter) {
  uint32_t primask = sys_log_enterexclusive();
  uint32_t value = *counter;
  sys_log_exitexclusive(primask);
  return value;
}

/**
 * @brief 在排他临界区内原子读取当前后端指针。
 * @return 当前绑定的后端指针（可能为 NULL）。
 */
static sys_log_base_t *sys_log_getsink(void) {
  uint32_t primask = sys_log_enterexclusive();
  sys_log_base_t *sink_h = s_log_sink;
  sys_log_exitexclusive(primask);
  return sink_h;
}

/**
 * @brief 根据当前后端与等级重算并刷新 g_sys_log_fast_mask（须在排他临界区内调用）。
 */
static void sys_log_updatefastmasklocked(void) {
  uint32_t mask = 0U;
  if (s_log_sink != NULL) {
    mask = SYS_LOG_FAST_READY_MASK_;
    if (s_log_level != SYS_LOG_LEVEL_NONE) {
      uint32_t level = (uint32_t)s_log_level;
      // 将 0..level 对应比特全部置 1，使热路径 Sys_Log_IsEnabled 仅需一次位与运算
      mask |= (1U << (level + 1U)) - 1U;
    }
  }
  g_sys_log_fast_mask = mask;
}

void Sys_Log_Init(sys_log_base_t *sink_h) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_sink = sink_h;
  s_drop_bytes = 0U;
  s_contention_count = 0U;
  s_log_locked = false;
  sys_log_updatefastmasklocked();
  sys_log_exitexclusive(primask);
}

void Sys_Log_SetSink(sys_log_base_t *sink_h) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_sink = sink_h;
  sys_log_updatefastmasklocked();
  sys_log_exitexclusive(primask);
}

void Sys_Log_SetLevel(sys_log_level_t level) {
  uint32_t primask = sys_log_enterexclusive();
  if ((uint32_t)level <= (uint32_t)SYS_LOG_LEVEL_NONE) {
    s_log_level = level;
  } else {
    s_log_level = SYS_LOG_LEVEL_NONE;
  }
  sys_log_updatefastmasklocked();
  sys_log_exitexclusive(primask);
}

sys_log_level_t Sys_Log_GetLevel(void) {
  uint32_t primask = sys_log_enterexclusive();
  sys_log_level_t level = s_log_level;
  sys_log_exitexclusive(primask);
  return level;
}

bool Sys_Log_IsReady(void) { return (g_sys_log_fast_mask & SYS_LOG_FAST_READY_MASK_) != 0U; }

void Sys_Log_Write(sys_log_level_t level, const char *tag, const char *format, ...) {
  char line[SYS_LOG_LINE_CAPACITY];

  if (tag == NULL || format == NULL || (uint32_t)level > (uint32_t)SYS_LOG_LEVEL_DEBUG ||
      !Sys_Log_IsEnabled(level)) {
    return;
  }

  // 非阻塞 try-lock：若其他任务正在写日志则直接丢弃并计数，避免高优先级任务发生优先级反转或等待
  if (!sys_log_trylock()) {
    sys_log_addcounter(&s_contention_count, 1U);
    return;
  }

  sys_log_base_t *sink_h = sys_log_getsink();
  if (sink_h == NULL) {
    sys_log_unlock();
    return;
  }

  uint32_t now_ms = SYS_LOG_GET_TIME_MS();
  int length = snprintf(line, sizeof(line), "[%lu][%s][%s] ", (unsigned long)now_ms,
                        sys_log_getleveltag(level), tag);
  if (length < 0 || (uint32_t)length >= sizeof(line)) {
    sys_log_unlock();
    return;
  }

  va_list args;
  va_start(args, format);
  int message_length = vsnprintf(&line[length], sizeof(line) - (uint32_t)length, format, args);
  va_end(args);

  if (message_length < 0) {
    sys_log_unlock();
    return;
  }

  // 预留末尾 2 字节给 "\r\n"（以及 snprintf 截断时保证不越界）
  uint32_t used = (uint32_t)length + (uint32_t)message_length;
  if (used > sizeof(line) - 3U) {
    used = sizeof(line) - 3U;
  }

  line[used++] = '\r';
  line[used++] = '\n';

  uint32_t accepted = Sys_Log_Base_Write(sink_h, (const uint8_t *)line, used);
  if (accepted < used) {
    sys_log_addcounter(&s_drop_bytes, used - accepted);
  }

  sys_log_unlock();
}

void Sys_Log_EmergencyRaw(const void *data, uint32_t size) {
  // 紧急路径跳过 try-lock 与 snprintf，直接在当前异常上下文向后端直写
  sys_log_base_t *sink_h = sys_log_getsink();
  if (sink_h == NULL || data == NULL || size == 0U) {
    return;
  }
  (void)Sys_Log_Base_Write(sink_h, (const uint8_t *)data, size);
}

uint32_t Sys_Log_GetDropBytes(void) { return sys_log_readcounter(&s_drop_bytes); }

uint32_t Sys_Log_GetContentionCount(void) { return sys_log_readcounter(&s_contention_count); }
