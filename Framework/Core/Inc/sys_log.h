/**
 * @file sys_log.h
 * @brief 非阻塞 System 日志门面（单行上限 192 byte，默认时间戳来自 HAL_GetTick）。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_LOG_H
#define LAYERED_CAR_SYS_LOG_H

#include "sys_log_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 日志严重等级。
 */
typedef enum sys_log_level_e {
  SYS_LOG_LEVEL_ERROR = 0,
  SYS_LOG_LEVEL_WARN,
  SYS_LOG_LEVEL_INFO,
  SYS_LOG_LEVEL_DEBUG,
  SYS_LOG_LEVEL_NONE,
} sys_log_level_t;

/** @cond SYS_LOG_INTERNAL */
extern volatile uint32_t g_sys_log_fast_mask;
/** @endcond */

/**
 * @brief 初始化日志门面并绑定后端（sink_h == NULL 表示关闭输出）。
 */
void Sys_Log_Init(sys_log_base_t *sink_h);

/**
 * @brief 热切换当前日志后端（不得在并发写入中释放旧后端）。
 */
void Sys_Log_SetSink(sys_log_base_t *sink_h);

/**
 * @brief 设置最低输出等级；超出范围的值按 SYS_LOG_LEVEL_NONE 处理。
 */
void Sys_Log_SetLevel(sys_log_level_t level);

/**
 * @brief 获取当前最低输出等级（默认 SYS_LOG_LEVEL_INFO）。
 */
sys_log_level_t Sys_Log_GetLevel(void);

/**
 * @brief 查询是否已配置有效后端。
 */
bool Sys_Log_IsReady(void);

/**
 * @brief 快速判断指定等级当前是否允许输出（被过滤时不进入临界区）。
 */
static inline bool Sys_Log_IsEnabled(sys_log_level_t level) {
  uint32_t level_index = (uint32_t)level;
  if (level_index > (uint32_t)SYS_LOG_LEVEL_DEBUG) {
    return false;
  }
  return (g_sys_log_fast_mask & (1U << level_index)) != 0U;
}

/**
 * @brief 格式化并提交日志（仅任务上下文调用；try-lock 冲突时直接丢弃）。
 */
void Sys_Log_Write(sys_log_level_t level, const char *tag, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

/**
 * @brief 绕过格式化与门面锁直写原始字节（仅用于 Fault 等最后兜底路径）。
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size);

/**
 * @brief 查询累计被后端拒绝的日志字节数，单位 byte。
 */
uint32_t Sys_Log_GetDropBytes(void);

/**
 * @brief 查询因锁竞争被丢弃的日志条数。
 */
uint32_t Sys_Log_GetContentionCount(void);

/** @brief 提交 ERROR 级日志（被过滤时参数不求值，仅任务上下文调用）。 */
#define SYS_LOG_ERROR(tag, ...)                                                                    \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR)) {                                                  \
      Sys_Log_Write(SYS_LOG_LEVEL_ERROR, (tag), __VA_ARGS__);                                      \
    }                                                                                              \
  } while (0)

/** @brief 提交 WARN 级日志（被过滤时参数不求值，仅任务上下文调用）。 */
#define SYS_LOG_WARN(tag, ...)                                                                     \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_WARN)) {                                                   \
      Sys_Log_Write(SYS_LOG_LEVEL_WARN, (tag), __VA_ARGS__);                                       \
    }                                                                                              \
  } while (0)

/** @brief 提交 INFO 级日志（被过滤时参数不求值，仅任务上下文调用）。 */
#define SYS_LOG_INFO(tag, ...)                                                                     \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_INFO)) {                                                   \
      Sys_Log_Write(SYS_LOG_LEVEL_INFO, (tag), __VA_ARGS__);                                       \
    }                                                                                              \
  } while (0)

/** @brief 提交 DEBUG 级日志（被过滤时参数不求值，仅任务上下文调用）。 */
#define SYS_LOG_DEBUG(tag, ...)                                                                    \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_DEBUG)) {                                                  \
      Sys_Log_Write(SYS_LOG_LEVEL_DEBUG, (tag), __VA_ARGS__);                                      \
    }                                                                                              \
  } while (0)

/** @} */

#endif /* LAYERED_CAR_SYS_LOG_H */
