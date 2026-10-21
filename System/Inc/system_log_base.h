//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file system_log_base.h
 * @brief System log output backend interface.
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_BASE_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_BASE_H

#include <stdint.h>

typedef struct sys_log_base sys_log_base_t;

/**
 * @brief Log backend operation table.
 * @note write must return before data becomes invalid, must not block, and
 *       returns the number of accepted bytes. A return of 0 drops the entry.
 */
typedef struct sys_log_ops {
  uint32_t (*write)(
    sys_log_base_t *this,
    const uint8_t *data,
    uint32_t size
  );
} sys_log_ops_t;

/**
 * @brief Log backend abstract object.
 */
typedef struct sys_log_base {
  const sys_log_ops_t *ops;
  const char          *name;
} sys_log_base_t;

/** @brief Initialize a log backend and bind its static operation table. */
void Sys_Log_Base_Init(
  sys_log_base_t *this,
  const char *name,
  const sys_log_ops_t *ops
);

/**
 * @brief Write bytes to a log backend.
 * @param this Backend object.
 * @param data Byte buffer.
 * @param size Byte count.
 * @return Number of accepted bytes; 0 when unavailable or dropped.
 */
uint32_t Sys_Log_Base_Write(
  sys_log_base_t *this,
  const uint8_t *data,
  uint32_t size
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_BASE_H
