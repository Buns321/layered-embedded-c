//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_adc.h
 * @brief HAL-free ADC runtime interface.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ADC_H
#define LAYERED_EMBEDDED_C_PLATFORM_ADC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_ADC1 && !PLATFORM_ENABLE_ADC2 && \
    !PLATFORM_ENABLE_ADC3
#error "At least one Platform ADC instance must be enabled"
#endif

/** @brief Available ADC instances. */
typedef enum {
#if PLATFORM_ENABLE_ADC1
  PLATFORM_ADC_1 = 0U,
#endif
#if PLATFORM_ENABLE_ADC2
  PLATFORM_ADC_2 = 1U,
#endif
#if PLATFORM_ENABLE_ADC3
  PLATFORM_ADC_3 = 2U,
#endif
} Platform_ADC_Instance;

/** @brief Number of possible ADC identity slots. */
#define PLATFORM_ADC_INSTANCE_COUNT 3U

/** @brief ADC error flags. Multiple bits may be set together. */
typedef enum {
  PLATFORM_ADC_ERROR_NONE     = 0u,
  PLATFORM_ADC_ERROR_INTERNAL = (1u << 0),
  PLATFORM_ADC_ERROR_OVERRUN  = (1u << 1),
  PLATFORM_ADC_ERROR_DMA      = (1u << 2),
  PLATFORM_ADC_ERROR_UNKNOWN  = (1u << 7),
} Platform_ADC_Error;

typedef void (*Platform_ADC_Callback)(
  Platform_ADC_Instance adc,
  void *context
);

/**
 * @brief ADC conversion callbacks.
 * @note Callbacks run in interrupt context.
 * @note One physical instance has one callback owner.
 */
typedef struct {
  Platform_ADC_Callback conv_cplt_cb;
  Platform_ADC_Callback conv_half_cplt_cb;
  Platform_ADC_Callback err_cb;
  void *context;
} platform_adc_callbacks_t;

/**
 * @brief Start conversions in polling mode.
 * @param adc ADC instance.
 * @pre MX_ADCx_Init() completed and regular-group channels are configured.
 */
Platform_StatusTypeDef Platform_ADC_Start(Platform_ADC_Instance adc);

/**
 * @brief Stop polling conversions.
 * @param adc ADC instance.
 */
Platform_StatusTypeDef Platform_ADC_Stop(Platform_ADC_Instance adc);

/**
 * @brief Wait for the current polling conversion to complete.
 * @param adc ADC instance.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE.
 */
Platform_StatusTypeDef Platform_ADC_PollForConversion(
  Platform_ADC_Instance adc,
  uint32_t timeout
);

/**
 * @brief Read the latest regular conversion result.
 * @param adc ADC instance.
 * @note The value is a raw ADC code unless CubeMX configured otherwise.
 */
uint32_t Platform_ADC_GetValue(Platform_ADC_Instance adc);

/**
 * @brief Perform one blocking regular-group conversion sequence.
 * @param adc ADC instance.
 * @param value Destination for the latest raw conversion result.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE.
 * @pre CubeMX configures a single-channel, single-conversion regular group.
 * @note For multi-channel scan mode, use DMA. The regular-group interrupt
 *       reports only the last conversion of a scan sequence.
 * @note The conversion is stopped before returning.
 */
Platform_StatusTypeDef Platform_ADC_Read(
  Platform_ADC_Instance adc,
  uint32_t *value,
  uint32_t timeout
);

/**
 * @brief Start conversions using the regular-group interrupt.
 * @param adc ADC instance.
 */
Platform_StatusTypeDef Platform_ADC_Start_IT(Platform_ADC_Instance adc);

/**
 * @brief Stop interrupt conversions.
 * @param adc ADC instance.
 */
Platform_StatusTypeDef Platform_ADC_Stop_IT(Platform_ADC_Instance adc);

/**
 * @brief Start regular-group DMA conversions.
 * @param adc ADC instance.
 * @param data Destination array of uint32_t conversion results.
 * @param length Number of conversions, not bytes.
 * @note data must remain valid until completion, error, or stop.
 * @note On STM32F1, only ADC1 has a DMA request. DMA start/stop on
 *       unsupported instances returns Platform_ERROR.
 */
Platform_StatusTypeDef Platform_ADC_Start_DMA(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
);

/**
 * @brief Stop DMA conversions.
 * @param adc ADC instance.
 * @note On STM32F1, stop is supported only for ADC1.
 */
Platform_StatusTypeDef Platform_ADC_Stop_DMA(Platform_ADC_Instance adc);

/**
 * @brief Read the latest HAL ADC error snapshot.
 * @param adc ADC instance.
 */
uint32_t Platform_ADC_GetLastError(Platform_ADC_Instance adc);

/**
 * @brief Register ADC callbacks.
 * @param adc ADC instance.
 * @param callbacks Callback set to copy.
 * @note Call once during initialization before starting IT/DMA conversions.
 */
void Platform_ADC_RegisterCallbacks(
  Platform_ADC_Instance adc,
  const platform_adc_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_H
