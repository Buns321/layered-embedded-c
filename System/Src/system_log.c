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
/**
 * @brief 可选的外部毫秒时间源。
 * @return 单调递增的毫秒时间。
 * @pre 集成方必须提供与声明一致的实现。
 * @note 仅在定义 `SYS_LOG_EXTERNAL_TIME_SOURCE` 时声明和调用。
 */
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
 * @return 进入前的 PRIMASK 快照；非 Cortex-M 构建返回 0。
 * @pre 只在短小的日志状态切换路径调用；不得在持锁期间执行阻塞操作。
 * @post Cortex-M 构建已屏蔽全局中断；返回值必须原样传给 sys_log_exitexclusive。
 * @note 私有函数，不分配动态内存；调用方负责与退出函数配对。
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
 * @param primask sys_log_enterexclusive 在同一上下文返回的 PRIMASK 快照。
 * @pre primask 必须来自同一上下文最近一次 sys_log_enterexclusive，且未被修改。
 * @post Cortex-M 构建的中断屏蔽状态已恢复到 primask；非 Cortex-M 构建不执行寄存器操作。
 * @note 私有函数，不分配动态内存；调用方必须与进入函数严格配对。
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
 * @param level 待转换的日志等级；允许 SYS_LOG_LEVEL_NONE 或越界值。
 * @return 指向静态只读字符串的非 NULL 指针：ERROR/WARN/INFO/DEBUG 分别为
 *         "E"/"W"/"I"/"D"，其他值为 "?"。
 * @note 私有函数，返回值具有进程生命周期；不分配动态内存，不修改共享状态。
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
 * @retval true 已取得软件锁，调用方必须在同一上下文调用 sys_log_unlock。
 * @retval false 其他上下文已持有软件锁，本次不等待。
 * @pre 只允许任务上下文使用普通日志路径调用；不得在 ISR 中竞争该锁。
 * @post 成功时 s_log_locked 置位；无论成功与否，进入前的 PRIMASK 状态均已恢复。
 * @note 私有函数，非阻塞尝试；不分配动态内存。
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
 * @pre 当前上下文持有 sys_log_trylock 成功取得的软件锁。
 * @post s_log_locked 已清零；进入前的 PRIMASK 状态已恢复。
 * @note 私有函数，不校验锁所有者且不分配动态内存；调用方必须保证配对。
 */
static void sys_log_unlock(void) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_locked = false;
  sys_log_exitexclusive(primask);
}

/**
 * @brief 原子累加日志诊断计数。
 * @param counter 非空静态 volatile 计数地址；生命周期覆盖整个程序运行期。
 * @param amount 增量，单位由目标计数器决定；0 表示不修改计数器。
 * @pre counter 非 NULL，且由本编译单元初始化为有效计数值。
 * @post amount 非 0 时计数器已原子增加 amount；32 位溢出按无符号语义回绕。
 * @note 私有函数，用 PRIMASK 临界区保护读改写；不分配动态内存。
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
 * @param counter 非空静态 volatile 计数地址；生命周期覆盖整个程序运行期。
 * @return 临界区内取得的一致计数值，单位由目标计数器决定。
 * @pre counter 非 NULL，且由本编译单元维护。
 * @post 计数器值不变；不取得或延长任何对象的生命周期。
 * @note 私有函数，不分配动态内存，可在任务上下文安全读取。
 */
static uint32_t sys_log_readcounter(volatile uint32_t *counter) {
  uint32_t primask = sys_log_enterexclusive();
  uint32_t value = *counter;
  sys_log_exitexclusive(primask);
  return value;
}

/**
 * @brief 获取当前日志后端。
 * @return 当前后端指针；未配置或已用 NULL 关闭输出时返回 NULL。
 * @post 返回的是临界区内取得的瞬时快照；不修改后端引用计数。
 * @note 私有函数，不分配动态内存。调用方使用指针期间必须保证后端生命周期，
 *       Sys_Log_SetSink 的热替换遵循公开接口的并发限制。
 */
static sys_log_base_t *sys_log_getsink(void) {
  uint32_t primask = sys_log_enterexclusive();
  sys_log_base_t *sink_h = s_log_sink;
  sys_log_exitexclusive(primask);
  return sink_h;
}

/**
 * @brief 在持锁状态下刷新快速日志级别掩码。
 * @pre 调用方已持有日志软件锁或处于 sys_log_enterexclusive 保护的临界区。
 * @post g_sys_log_fast_mask 已根据 s_log_sink 和 s_log_level 重新生成；
 *       后端为 NULL 时为 0，否则置 ready 位并按等级设置允许位。
 * @note 私有函数，不分配动态内存；不得在未持有互斥保护时调用。
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
 * @param sink_h 后端对象；传入 NULL 表示关闭输出，但仍保留门面状态可查询。
 * @pre 应在 HAL 毫秒时基初始化完成后、并发写入开始前调用；非 NULL sink_h 的生命周期
 *      必须覆盖日志门面的全部使用期。
 * @post 当前后端已替换为 sink_h，丢弃字节数和竞争计数清零，软件锁释放，快速等级掩码
 *       按当前等级重新生成。
 * @note 不分配动态内存；只在短暂的 PRIMASK 临界区内切换状态，不得与正在进行的日志写入并发调用。
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
 * @param sink_h 新的后端对象；传入 NULL 表示关闭输出。
 * @pre 非 NULL sink_h 必须已初始化，且其生命周期覆盖日志门面的剩余使用期。
 * @post 后续日志使用新后端并刷新快速等级掩码；统计计数和当前等级保持不变。
 * @note 替换只保证后续日志使用新后端；已经开始的写入仍可能使用旧后端，
 *       因此旧后端必须在并发写入结束前保持有效。
 * @warning 本函数不等待正在进行的写入，不能用于释放旧后端存储后的热替换。
 */
void Sys_Log_SetSink(sys_log_base_t *sink_h) {
  uint32_t primask = sys_log_enterexclusive();
  s_log_sink = sink_h;
  sys_log_updatefastmasklocked();
  sys_log_exitexclusive(primask);
}

/**
 * @brief 设置运行期最低输出等级。
 * @param level 最低严重等级，该等级及更严重等级会被输出；有效值为
 *              SYS_LOG_LEVEL_ERROR..SYS_LOG_LEVEL_NONE。
 * @post 有效等级已保存并刷新快速掩码；超出枚举范围的值按 SYS_LOG_LEVEL_NONE 处理。
 * @note 不分配动态内存；状态切换由短暂 PRIMASK 临界区保护。
 * @warning 无效枚举值会关闭日志输出，且不会返回错误。
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

/**
 * @brief 读取当前配置的最低输出等级。
 * @return 当前最低输出等级，取值 SYS_LOG_LEVEL_ERROR..SYS_LOG_LEVEL_NONE。
 * @note 返回在短暂 PRIMASK 临界区内取得的一致快照；尚未设置过时默认 INFO。
 */
sys_log_level_t Sys_Log_GetLevel(void) {
  uint32_t primask = sys_log_enterexclusive();
  sys_log_level_t level = s_log_level;
  sys_log_exitexclusive(primask);
  return level;
}

/**
 * @brief 判断后端是否已经配置。
 * @retval true 后端已配置，即使当前等级为 NONE。
 * @retval false 后端为 NULL，尚未配置或已关闭输出。
 * @note 只读取快速掩码中的 ready 位，不进入临界区；结果是瞬时快照。
 */
bool Sys_Log_IsReady(void) {
  return (g_sys_log_fast_mask & SYS_LOG_FAST_READY_MASK_) != 0u;
}

/**
 * @brief 格式化并提交一条日志。
 * @param level 本条日志的严重等级；仅 ERROR..DEBUG 会尝试输出。
 * @param tag 非空只读子系统名称，只在本条日志格式化期间使用。
 * @param format 非空 printf 风格格式字符串，只在本次调用期间使用。
 * @param ... 与 format 匹配的可变参数，由调用方保证类型和数量正确。
 * @pre 在任务上下文调用；tag 和 format 非 NULL，level 在 ERROR..DEBUG 范围内。
 * @post 日志已提交、因门面忙碌被丢弃、因等级/参数无效被忽略，或后端只接受部分字节；
 *       发生丢弃时更新竞争计数或丢弃字节计数。
 * @note 非阻塞；使用 try-lock，格式化或写入期间其他写入者会直接丢弃本条日志。
 *       单条输出缓冲上限为 192 byte，超长内容会被截断；不分配动态内存。
 * @warning 不在 ISR 中调用。常规日志避免格式化浮点数；高频或浮点数据应使用遥测。
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
 * @param data 非空只读原始字节缓冲区，只在本次调用期间访问。
 * @param size 待提交字节数，单位 byte，必须大于 0。
 * @pre 后端至少已设置一次；调用方保证 data 非 NULL 且 size > 0。
 * @post 数据已直接交给当前后端；未接受部分不会计入普通日志丢弃统计。
 * @note 用于 Fault 等最后兜底路径。该接口不参与日志门面互斥，调用方必须
 *       保证不会与普通写入并发，或为紧急路径提供独立后端缓冲区。
 * @warning 与普通写入并发时可能破坏后端缓冲区的原子性；后端缺失或参数无效时静默丢弃。
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

/**
 * @brief 当前后端拒绝的字节数。
 * @return 自最近一次 Sys_Log_Init 起累计未接受的普通日志字节数，单位 byte；
 *         32 位无符号计数溢出后回绕。
 * @note 在短暂 PRIMASK 临界区内读取一致快照；不包含 Sys_Log_EmergencyRaw 的丢弃量。
 */
uint32_t Sys_Log_GetDropBytes(void) {
  return sys_log_readcounter(&s_drop_bytes);
}

/**
 * @brief 因其他写入者占用日志门面而被拒绝的日志条数。
 * @return 自最近一次 Sys_Log_Init 起因 try-lock 失败而丢弃的 Sys_Log_Write 调用条数；
 *         32 位无符号计数溢出后回绕。
 * @note 在短暂 PRIMASK 临界区内读取一致快照；参数无效或等级过滤不计入该计数。
 */
uint32_t Sys_Log_GetContentionCount(void) {
  return sys_log_readcounter(&s_contention_count);
}
