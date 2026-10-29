//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log.h
 * @brief 非阻塞 System 日志门面。
 * @addtogroup system
 * @{
 * @note 默认时间戳来自 STM32 HAL 毫秒时基。其他平台可在编译选项中定义
 *       `SYS_LOG_EXTERNAL_TIME_SOURCE=<函数名>`，该函数返回单调毫秒时间；
 *       也可直接定义 `SYS_LOG_GET_TIME_MS()` 覆盖默认实现。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_H

#include "system_log_base.h"
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
 * @brief 初始化日志门面。
 * @param sink_h 后端对象；传入 NULL 可关闭输出。
 * @note 应在 HAL 初始化完成后、并发写入开始前调用；调用会清空统计值。
 */
void Sys_Log_Init(sys_log_base_t *sink_h);

/**
 * @brief 替换当前生效的后端。
 * @param sink_h 新的后端对象；传入 NULL 可关闭输出。
 * @note 替换只保证后续日志使用新后端；已经开始的写入仍可能使用旧后端，
 *       因此旧后端必须在并发写入结束前保持有效。
 */
void Sys_Log_SetSink(sys_log_base_t *sink_h);

/**
 * @brief 设置运行期最低输出等级。
 * @param level 最低严重等级，该等级及更严重等级会被输出；
 *              使用 SYS_LOG_LEVEL_NONE 可关闭日志。
 */
void Sys_Log_SetLevel(sys_log_level_t level);

/** @brief 读取当前配置的最低输出等级。 */
sys_log_level_t Sys_Log_GetLevel(void);

/**
 * @brief 判断后端是否已经配置。
 * @retval true 后端已配置，即使当前等级为 NONE。
 * @retval false 后端尚未配置。
 */
bool Sys_Log_IsReady(void);

/**
 * @brief 判断指定等级当前是否会输出。
 * @param level 待检查的日志等级。
 * @return 该等级当前允许输出时返回 true。
 * @note 该函数位于头文件中，关闭日志时只读取一次快速掩码，不进入临界区。
 *       日志宏先调用本函数，被过滤的日志不会对参数表达式求值。
 */
static inline bool Sys_Log_IsEnabled(sys_log_level_t level) {
  uint32_t level_index = (uint32_t)level;

  if (level_index > (uint32_t)SYS_LOG_LEVEL_DEBUG) {
    return false;
  }

  return (g_sys_log_fast_mask & (1u << level_index)) != 0u;
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
)
#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
;

/**
 * @brief 不经格式化和加锁，直接提交原始字节。
 * @param data 原始字节缓冲区。
 * @param size 字节数，必须大于 0。
 * @note 用于 Fault 等最后兜底路径。该接口不参与日志门面互斥，调用方必须
 *       保证不会与普通写入并发，或为紧急路径提供独立后端缓冲区。
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size);

/** @brief 当前后端拒绝的字节数。 */
uint32_t Sys_Log_GetDropBytes(void);

/** @brief 因其他写入者占用日志门面而被拒绝的日志条数。 */
uint32_t Sys_Log_GetContentionCount(void);

/**
 * @brief 按 ERROR 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_ERROR(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)
/**
 * @brief 按 WARN 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_WARN(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_WARN)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)
/**
 * @brief 按 INFO 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_INFO(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_INFO)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)
/**
 * @brief 按 DEBUG 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_DEBUG(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_DEBUG)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_H
