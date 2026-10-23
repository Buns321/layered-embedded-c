//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log.h
 * @brief 非阻塞 System 日志门面。
 * @addtogroup system
 * @{
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

/**
 * @brief 初始化日志门面。
 * @param sink_h 后端对象；传入 NULL 可关闭输出。
 * @note 应在 HAL 初始化完成后，从任务上下文调用。
 */
void Sys_Log_Init(sys_log_base_t *sink_h);

/** @brief 替换当前生效的后端。 */
void Sys_Log_SetSink(sys_log_base_t *sink_h);

/**
 * @brief 设置运行期最低输出等级。
 * @param level 最低等级；使用 SYS_LOG_LEVEL_NONE 可关闭日志。
 */
void Sys_Log_SetLevel(sys_log_level_t level);

/** @brief 读取当前配置的最低输出等级。 */
sys_log_level_t Sys_Log_GetLevel(void);

/** @brief 后端已配置时返回 true。 */
bool Sys_Log_IsReady(void);

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
);

/**
 * @brief 不经格式化和加锁，直接提交原始字节。
 * @note 用于 Fault 等最后兜底路径。
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size);

/** @brief 当前后端拒绝的字节数。 */
uint32_t Sys_Log_GetDropBytes(void);

/** @brief 因其他写入者占用日志门面而被拒绝的日志条数。 */
uint32_t Sys_Log_GetContentionCount(void);

#define SYS_LOG_ERROR(tag, format, ...) \
  Sys_Log_Write(SYS_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__)
#define SYS_LOG_WARN(tag, format, ...) \
  Sys_Log_Write(SYS_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__)
#define SYS_LOG_INFO(tag, format, ...) \
  Sys_Log_Write(SYS_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__)
#define SYS_LOG_DEBUG(tag, format, ...) \
  Sys_Log_Write(SYS_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_H
