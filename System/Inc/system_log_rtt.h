//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file system_log_rtt.h
 * @brief SEGGER RTT implementation of the System log backend.
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H

#include "system_log_base.h"
#include <stdint.h>

/**
 * @brief RTT log backend object.
 * @note The object must be static or otherwise outlive the System log facade.
 */
typedef struct {
  sys_log_base_t base;
  uint32_t       channel;
} system_log_rtt_t;

/**
 * @brief Initialize the RTT log backend.
 * @param this Backend object.
 * @param name Debug name.
 * @param channel RTT up-channel, normally 0.
 */
void Sys_Log_RTT_Init(
  system_log_rtt_t *this,
  const char *name,
  uint32_t channel
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_RTT_H
