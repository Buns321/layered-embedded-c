//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_dac.h
 * @brief HAL-free DAC output interface.
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_DAC1
#error "Platform DAC1 must be enabled before including platform_dac.h"
#endif

/** @brief Available DAC instances. */
typedef enum {
  PLATFORM_DAC_1 = 0U,
} Platform_DAC_Instance;

/** @brief Number of possible DAC identity slots. */
#define PLATFORM_DAC_INSTANCE_COUNT 1U

/** @brief DAC output channels. */
typedef enum {
  PLATFORM_DAC_CHANNEL_1 = 0U,
  PLATFORM_DAC_CHANNEL_2 = 1U,
} Platform_DAC_Channel;

/** @brief Value alignment used by the configured DAC channel. */
typedef enum {
  PLATFORM_DAC_ALIGN_8_BIT_RIGHT = 0U,
  PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
  PLATFORM_DAC_ALIGN_12_BIT_LEFT,
} Platform_DAC_Alignment;

/** @brief DAC error flags. */
typedef enum {
  PLATFORM_DAC_ERROR_NONE          = 0u,
  PLATFORM_DAC_ERROR_DMA_UNDERRUN  = (1u << 0),
  PLATFORM_DAC_ERROR_DMA           = (1u << 1),
  PLATFORM_DAC_ERROR_TIMEOUT       = (1u << 2),
  PLATFORM_DAC_ERROR_UNKNOWN       = (1u << 7),
} Platform_DAC_Error;

typedef void (*Platform_DAC_Callback)(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  void *context
);

/**
 * @brief DAC conversion callbacks.
 * @note Callbacks run in interrupt context.
 * @note One physical DAC instance has one callback owner.
 */
typedef struct {
  Platform_DAC_Callback conv_cplt_cb;
  Platform_DAC_Callback conv_half_cplt_cb;
  Platform_DAC_Callback err_cb;
  Platform_DAC_Callback underrun_cb;
  void *context;
} platform_dac_callbacks_t;

/**
 * @brief Set a DAC output register value without starting the channel.
 * @param dac DAC instance.
 * @param channel DAC output channel.
 * @param alignment Value alignment.
 * @param value Raw code whose width is described by alignment.
 */
Platform_StatusTypeDef Platform_DAC_SetValue(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  uint32_t value
);

/**
 * @brief Start a DAC channel with the CubeMX trigger configuration.
 * @param dac DAC instance.
 * @param channel DAC output channel.
 */
Platform_StatusTypeDef Platform_DAC_Start(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
);

/**
 * @brief Stop a DAC channel.
 * @param dac DAC instance.
 * @param channel DAC output channel.
 */
Platform_StatusTypeDef Platform_DAC_Stop(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
);

/**
 * @brief Start DMA output for a DAC channel.
 * @param dac DAC instance.
 * @param channel DAC output channel.
 * @param alignment Alignment of every sample in data.
 * @param data Array of raw uint32_t samples. Samples follow the alignment used
 *             by the CubeMX DAC channel configuration.
 * @param length Number of samples, not bytes.
 * @note data must remain valid until completion, error, or stop.
 */
Platform_StatusTypeDef Platform_DAC_Start_DMA(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel,
  Platform_DAC_Alignment alignment,
  const uint32_t *data,
  uint32_t length
);

/**
 * @brief Stop DMA output for a DAC channel.
 * @param dac DAC instance.
 * @param channel DAC output channel.
 */
Platform_StatusTypeDef Platform_DAC_Stop_DMA(
  Platform_DAC_Instance dac,
  Platform_DAC_Channel channel
);

/**
 * @brief Read the latest HAL DAC error snapshot.
 * @param dac DAC instance.
 */
uint32_t Platform_DAC_GetLastError(Platform_DAC_Instance dac);

/**
 * @brief Register DAC callbacks.
 * @param dac DAC instance.
 * @param callbacks Callback set to copy.
 * @note Call once during initialization before starting DMA output.
 */
void Platform_DAC_RegisterCallbacks(
  Platform_DAC_Instance dac,
  const platform_dac_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_H
