//
// Created by Buns_ on 2026/10/22.
//

/**
 * @file platform_wwdg.h
 * @brief Window watchdog start, refresh and reset flags.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_WWDG_H
#define LAYERED_EMBEDDED_C_PLATFORM_WWDG_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_WWDG
#error "Platform WWDG is not enabled"
#endif

/** @brief Available WWDG identity. */
typedef enum {
  PLATFORM_WWDG_1 = 0U,
} Platform_WWDG_Instance;

/** @brief Minimum valid raw window value. */
#define PLATFORM_WWDG_WINDOW_MIN 0x40u
/** @brief Maximum valid raw window value. */
#define PLATFORM_WWDG_WINDOW_MAX 0x7Fu
/** @brief Maximum conservative timeout for the current 42 MHz PCLK1. */
#define PLATFORM_WWDG_MAX_TIMEOUT_MS 49u

/**
 * @brief Runtime WWDG configuration.
 */
typedef struct {
  uint32_t timeout_ms; /**< Requested timeout, 1..PLATFORM_WWDG_MAX_TIMEOUT_MS. */
  uint8_t window;      /**< Raw refresh window, PLATFORM_WWDG_WINDOW_MIN..MAX. */
} Platform_WWDG_Config;

/**
 * @brief Start the window watchdog.
 * @param wwdg WWDG instance.
 * @param config Runtime configuration.
 * @return Platform status.
 * @note This is a runtime policy operation and starts the watchdog immediately.
 * @note WWDG cannot be stopped after this call.
 * @note Refresh after the counter falls below window and before it reaches 0x3F.
 */
Platform_StatusTypeDef Platform_WWDG_Start(
  Platform_WWDG_Instance wwdg,
  const Platform_WWDG_Config *config
);

/**
 * @brief Refresh a started WWDG.
 * @return Platform_OK on success, Platform_ERROR if WWDG was not started.
 */
Platform_StatusTypeDef Platform_WWDG_Refresh(
  Platform_WWDG_Instance wwdg
);

/**
 * @brief Read and clear the WWDG reset flag.
 * @return true when the last reset was caused by WWDG.
 * @note Clearing reset flags clears the shared STM32 reset-flag latch.
 */
bool Platform_WWDG_WasReset(Platform_WWDG_Instance wwdg);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_H
