/**
 * @file platform_wwdg_backend.h
 * @brief STM32 系列公共 WWDG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H

#include "platform_def.h"
#include "platform_wwdg.h"
#include <stdbool.h>
#include <stdint.h>

uint32_t platform_wwdg_backend_pclk1_hz(void);

platform_status_t platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
);

platform_status_t platform_wwdg_backend_refresh(void);

bool platform_wwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
