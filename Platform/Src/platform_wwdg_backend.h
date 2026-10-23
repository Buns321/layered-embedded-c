/**
 * @file platform_wwdg_backend.h
 * @brief Private STM32-family backend for the common WWDG implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H

#include "platform_def.h"
#include "platform_wwdg.h"
#include <stdbool.h>
#include <stdint.h>

uint32_t platform_wwdg_backend_pclk1_hz(void);

Platform_StatusTypeDef platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
);

Platform_StatusTypeDef platform_wwdg_backend_refresh(void);

bool platform_wwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
