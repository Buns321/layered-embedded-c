//
// Created by Buns_ on 2026/10/22.
//

/**
 * @file platform_iwdg.h
 * @brief Independent watchdog start, refresh and reset flags.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_IWDG_H
#define LAYERED_EMBEDDED_C_PLATFORM_IWDG_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_IWDG
#error "Platform IWDG is not enabled"
#endif

/** @brief Available IWDG identity. */
typedef enum {
  PLATFORM_IWDG_1 = 0U,
} Platform_IWDG_Instance;

/** @brief Maximum requested timeout using the family's typical LSI. */
#if defined(STM32F1xx)
#define PLATFORM_IWDG_MAX_TIMEOUT_MS 26208u
#elif defined(STM32F4xx)
#define PLATFORM_IWDG_MAX_TIMEOUT_MS 32760u
#else
#error "Unsupported STM32 family for IWDG"
#endif

/**
 * @brief Start the independent watchdog.
 * @param iwdg IWDG instance.
 * @param timeout_ms Requested timeout in milliseconds, 1..PLATFORM_IWDG_MAX_TIMEOUT_MS.
 * @return Platform status.
 * @note This is a runtime policy operation and starts the watchdog immediately.
 * @note IWDG cannot be stopped after this call.
 * @note The timeout is rounded up and depends on LSI tolerance.
 */
Platform_StatusTypeDef Platform_IWDG_Start(
  Platform_IWDG_Instance iwdg,
  uint32_t timeout_ms
);

/**
 * @brief Refresh a started IWDG.
 * @return Platform_OK on success, Platform_ERROR if IWDG was not started.
 */
Platform_StatusTypeDef Platform_IWDG_Refresh(
  Platform_IWDG_Instance iwdg
);

/**
 * @brief Read and clear the IWDG reset flag.
 * @return true when the last reset was caused by IWDG.
 * @note Clearing reset flags clears the shared STM32 reset-flag latch.
 */
bool Platform_IWDG_WasReset(Platform_IWDG_Instance iwdg);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_IWDG_H
