//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log.c
 * @brief System 日志门面实现。
 * @ingroup system
 */

#include "system_log.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#endif

#ifndef SYS_LOG_GET_TIME_MS
#if defined(SYS_LOG_EXTERNAL_TIME_SOURCE)
extern uint32_t SYS_LOG_EXTERNAL_TIME_SOURCE(void);
#define SYS_LOG_GET_TIME_MS() SYS_LOG_EXTERNAL_TIME_SOURCE()
#elif defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#define SYS_LOG_GET_TIME_MS() HAL_GetTick()
#elif defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#define SYS_LOG_GET_TIME_MS() HAL_GetTick()
#else
#define SYS_LOG_GET_TIME_MS() 0u
#endif
#endif

#define SYS_LOG_LINE_CAPACITY 192u
#define SYS_LOG_FAST_READY_MASK_ (1u << 31)

volatile uint32_t g_sys_log_fast_mask;
static sys_log_base_t *volatile s_log_sink;
static sys_log_level_t volatile s_log_level = SYS_LOG_LEVEL_INFO;
static volatile bool s_log_locked;
static volatile uint32_t s_drop_bytes;
static volatile uint32_t s_contention_count;

/**
 * @brief 进入日志临界区并返回恢复状态。
 */
static uint32_t sys_log_enterexclusive(void) {
#if defined(STM32F4xx) || defined(STM32F1xx)
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
#else
  return 0u;
#endif
}

/**
 * @brief 按保存的状态退出日志临界区。
 */
static void sys_log_exitexclusive(uint32_t primask) {
#if defined(STM32F4xx) || defined(STM32F1xx)
  __set_PRIMASK(primask);
#else
  (void)primask;
#endif
}

/**
 * @brief 返回日志级别的固定文本标签。
 */
static const char *sys_log_getleveltag(sys_log_level_t level) {
  switch (level) {
    case SYS_LOG_LEVEL_ERROR: return "E";
    case SYS_LOG_LEVEL_WARN:  return "W";
    case SYS_LOG_LEVEL_INFO:  return "I";
    case SYS_LOG_LEVEL_DEBUG: return "D";
    default:                  return "?";
  }
}

/**
 * @brief 尝试获取日志锁。
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
 * @brief 释放日志锁。
 */
static void sys_log_unlock(void) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_locked = false;
  sys_log_exitexclusive(primask);
}

/**
 * @brief 原子累加日志诊断计数。
 */
static void sys_log_addcounter(
  volatile uint32_t *counter,
  uint32_t amount
) {
  if (amount == 0u) {
    return;
  }

  uint32_t primask = sys_log_enterexclusive();
  *counter += amount;
  sys_log_exitexclusive(primask);
}

/**
 * @brief 原子读取日志诊断计数。
 */
static uint32_t sys_log_readcounter(volatile uint32_t *counter) {
  uint32_t primask = sys_log_enterexclusive();
  uint32_t value = *counter;
  sys_log_exitexclusive(primask);
  return value;
}

/**
 * @brief 获取当前日志后端。
 */
static sys_log_base_t *sys_log_getsink(void) {
  uint32_t primask = sys_log_enterexclusive();
  sys_log_base_t *sink_h = s_log_sink;
  sys_log_exitexclusive(primask);
  return sink_h;
}

/**
 * @brief 在持锁状态下刷新快速日志级别掩码。
 */
static void sys_log_updatefastmasklocked(void) {
  uint32_t mask = 0u;

  if (s_log_sink != NULL) {
    mask = SYS_LOG_FAST_READY_MASK_;

    if (s_log_level != SYS_LOG_LEVEL_NONE) {
      uint32_t level = (uint32_t)s_log_level;
      mask |= (1u << (level + 1u)) - 1u;
    }
  }

  g_sys_log_fast_mask = mask;
}

/**
 * @brief 初始化日志门面。
 * @param sink_h 后端对象；传入 NULL 可关闭输出。
 * @note 应在 HAL 初始化完成后、并发写入开始前调用；调用会清空统计值。
 */
void Sys_Log_Init(sys_log_base_t *sink_h) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_sink = sink_h;
  s_drop_bytes = 0u;
  s_contention_count = 0u;
  s_log_locked = false;
  sys_log_updatefastmasklocked();
  sys_log_exitexclusive(primask);
}

/**
 * @brief 替换当前生效的后端。
 * @param sink_h 新的后端对象；传入 NULL 可关闭输出。
 * @note 替换只保证后续日志使用新后端；已经开始的写入仍可能使用旧后端，
 *       因此旧后端必须在并发写入结束前保持有效。
 */
void Sys_Log_SetSink(sys_log_base_t *sink_h) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_sink = sink_h;
  sys_log_updatefastmasklocked();
  sys_log_exitexclusive(primask);
}

/**
 * @brief 设置运行期最低输出等级。
 * @param level 最低严重等级，该等级及更严重等级会被输出；
 *              使用 SYS_LOG_LEVEL_NONE 可关闭日志。
 */
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

/** @brief 读取当前配置的最低输出等级。 */
sys_log_level_t Sys_Log_GetLevel(void) {
  uint32_t primask = sys_log_enterexclusive();
  sys_log_level_t level = s_log_level;
  sys_log_exitexclusive(primask);
  return level;
}

/**
 * @brief 判断后端是否已经配置。
 * @retval true 后端已配置，即使当前等级为 NONE。
 * @retval false 后端尚未配置。
 */
bool Sys_Log_IsReady(void) {
  return (g_sys_log_fast_mask & SYS_LOG_FAST_READY_MASK_) != 0u;
}

/**
 * @brief 格式化并提交一条日志。
 * @param level 本条日志的严重等级。
 * @param tag 简短的静态子系统名称。
 * @param format printf 风格的格式字符串。
 * @note 仅允许任务上下文调用。该调用非阻塞；其他任务正在格式化或后端
 *       空间不足时会丢弃本条日志。
 * @warning 常规日志中避免格式化浮点数。高频或浮点数据应使用遥测。
 */
void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
) {
  char line[SYS_LOG_LINE_CAPACITY];

  if (
    tag == NULL    ||
    format == NULL ||
    (uint32_t)level > (uint32_t)SYS_LOG_LEVEL_DEBUG ||
    !Sys_Log_IsEnabled(level)
  ) {
    return;
  }

  if (!sys_log_trylock()) {
    sys_log_addcounter(&s_contention_count, 1u);
    return;
  }

  sys_log_base_t *sink_h = sys_log_getsink();
  if (sink_h == NULL) {
    sys_log_unlock();
    return;
  }

  uint32_t now_ms = SYS_LOG_GET_TIME_MS();
  int length = snprintf(
    line,
    sizeof(line),
    "[%lu][%s][%s] ",
    (unsigned long)now_ms,
    sys_log_getleveltag(level),
    tag
  );

  if (length < 0 || (uint32_t)length >= sizeof(line)) {
    sys_log_unlock();
    return;
  }

  va_list args;
  va_start(args, format);
  int message_length = vsnprintf(
    &line[length],
    sizeof(line) - (uint32_t)length,
    format,
    args
  );
  va_end(args);

  if (message_length < 0) {
    sys_log_unlock();
    return;
  }

  uint32_t used = (uint32_t)length + (uint32_t)message_length;
  if (used > sizeof(line) - 3u) {
    used = sizeof(line) - 3u;
  }

  line[used++] = '\r';
  line[used++] = '\n';

  uint32_t accepted = Sys_Log_Base_Write(
    sink_h,
    (const uint8_t *)line,
    used
  );

  if (accepted < used) {
    sys_log_addcounter(&s_drop_bytes, used - accepted);
  }

  sys_log_unlock();
}

/**
 * @brief 不经格式化和加锁，直接提交原始字节。
 * @param data 原始字节缓冲区。
 * @param size 字节数，必须大于 0。
 * @note 用于 Fault 等最后兜底路径。该接口不参与日志门面互斥，调用方必须
 *       保证不会与普通写入并发，或为紧急路径提供独立后端缓冲区。
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size) {
  sys_log_base_t *sink_h = sys_log_getsink();

  if (
    sink_h == NULL ||
    data == NULL   ||
    size == 0u
  ) {
    return;
  }

  (void)Sys_Log_Base_Write(
    sink_h,
    (const uint8_t *)data,
    size
  );
}

/** @brief 当前后端拒绝的字节数。 */
uint32_t Sys_Log_GetDropBytes(void) {
  return sys_log_readcounter(&s_drop_bytes);
}

/** @brief 因其他写入者占用日志门面而被拒绝的日志条数。 */
uint32_t Sys_Log_GetContentionCount(void) {
  return sys_log_readcounter(&s_contention_count);
}
