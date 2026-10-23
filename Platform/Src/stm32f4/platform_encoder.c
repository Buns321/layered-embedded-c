/**
 * @file platform_encoder.c
 * @brief STM32F4 HAL backend for the common encoder implementation.
 */

#include "platform_encoder_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

static TIM_HandleTypeDef *platform_encoder_to_hal(
  platform_encoder_instance_t encoder
) {
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: return &htim1;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: return &htim2;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: return &htim3;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: return &htim4;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
    case PLATFORM_ENCODER_TIM5: return &htim5;
#endif
    default: return NULL;
  }
}

bool platform_encoder_backend_is_valid(
  platform_encoder_instance_t encoder
) {
  return platform_encoder_to_hal(encoder) != NULL;
}

platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  return encoder_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_Encoder_Start(encoder_h, TIM_CHANNEL_ALL)
                     );
}

platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  return encoder_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_Encoder_Stop(encoder_h, TIM_CHANNEL_ALL)
                     );
}

platform_status_t platform_encoder_backend_get_counter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  if (encoder_h == NULL || counter == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  *counter = __HAL_TIM_GET_COUNTER(encoder_h);
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_encoder_backend_set_counter(
  platform_encoder_instance_t encoder,
  uint32_t counter
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  if (encoder_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  __HAL_TIM_SET_COUNTER(encoder_h, counter);
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_encoder_backend_get_counter_bits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
) {
  if (counter_bits == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: *counter_bits = 16U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: *counter_bits = 32U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: *counter_bits = 16U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: *counter_bits = 16U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
    case PLATFORM_ENCODER_TIM5: *counter_bits = 32U; return PLATFORM_STATUS_OK;
#endif
    default: return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
}
