/**
 * @file platform_iwdg_backend.h
 * @brief STM32 系列公共 IWDG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H

#include "platform_def.h"
#include "platform_iwdg.h"
#include <stdbool.h>
#include <stdint.h>

uint32_t platform_iwdg_backend_lsi_hz(void);

platform_status_t platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
);

platform_status_t platform_iwdg_backend_refresh(void);

bool platform_iwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H
