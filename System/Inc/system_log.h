//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file system_log.h
 * @brief Non-blocking system log facade.
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_H

#include "system_log_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Log severity.
 */
typedef enum {
  SYS_LOG_LEVEL_ERROR = 0,
  SYS_LOG_LEVEL_WARN,
  SYS_LOG_LEVEL_INFO,
  SYS_LOG_LEVEL_DEBUG,
  SYS_LOG_LEVEL_NONE,
} Sys_Log_Level;

/**
 * @brief Initialize the log facade.
 * @param sink Backend object, or NULL to disable output.
 * @note Call from task context after HAL initialization.
 */
void Sys_Log_Init(sys_log_base_t *sink);

/** @brief Replace the active backend. */
void Sys_Log_SetSink(sys_log_base_t *sink);

/**
 * @brief Set the runtime minimum severity.
 * @param level Minimum level; use SYS_LOG_LEVEL_NONE to disable logging.
 */
void Sys_Log_SetLevel(Sys_Log_Level level);

/** @brief Read the configured minimum severity. */
Sys_Log_Level Sys_Log_GetLevel(void);

/** @brief Return true when a backend is configured. */
bool Sys_Log_IsReady(void);

/**
 * @brief Format and submit one log entry.
 * @param level Entry severity.
 * @param tag Short static subsystem name.
 * @param format printf-style format string.
 * @note Task context only. The call is non-blocking and drops the entry when
 *       another task is formatting or the backend has no space.
 * @warning Avoid floating-point formatting in normal logs. Use telemetry for
 *          high-rate or floating-point data.
 */
void Sys_Log_Write(
  Sys_Log_Level level,
  const char *tag,
  const char *format,
  ...
);

/**
 * @brief Submit raw bytes without formatting or locking.
 * @note Intended for Fault and other last-resort paths.
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size);

/** @brief Bytes rejected by the active backend. */
uint32_t Sys_Log_GetDropBytes(void);

/** @brief Entries rejected because another writer held the facade. */
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
