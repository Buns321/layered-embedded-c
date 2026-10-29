/**
 * @file platform_adc.c
 * @brief STM32F4 公共 ADC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "adc.h"
#include "platform_adc_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把平台 ADC 实例编号转换为 HAL 句柄。
 */
static ADC_HandleTypeDef *platform_adc_to_hal(platform_adc_instance_t adc) {
  switch (adc) {
#if PLATFORM_ENABLE_ADC1
    case PLATFORM_ADC_1: return &hadc1;
#endif
#if PLATFORM_ENABLE_ADC2
    case PLATFORM_ADC_2: return &hadc2;
#endif
#if PLATFORM_ENABLE_ADC3
    case PLATFORM_ADC_3: return &hadc3;
#endif
    default: return NULL;
  }
}

/**
 * @brief 把 HAL ADC 句柄识别为平台实例编号。
 */
static bool platform_hal_to_adc(
  ADC_HandleTypeDef *adc_h,
  platform_adc_instance_t *adc
) {
  if (adc_h == NULL || adc == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_ADC1
  if (adc_h == &hadc1) {
    *adc = PLATFORM_ADC_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_ADC2
  if (adc_h == &hadc2) {
    *adc = PLATFORM_ADC_2;
    return true;
  }
#endif
#if PLATFORM_ENABLE_ADC3
  if (adc_h == &hadc3) {
    *adc = PLATFORM_ADC_3;
    return true;
  }
#endif
  return false;
}

/**
 * @brief 把 HAL ADC 错误位映射为平台错误码。
 */
static uint32_t platform_adc_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_ADC_ERROR_NONE;
  if ((hal_error & HAL_ADC_ERROR_INTERNAL) != 0U) {
    error |= PLATFORM_ADC_ERROR_INTERNAL;
  }
  if ((hal_error & HAL_ADC_ERROR_OVR) != 0U) {
    error |= PLATFORM_ADC_ERROR_OVERRUN;
  }
  if ((hal_error & HAL_ADC_ERROR_DMA) != 0U) {
    error |= PLATFORM_ADC_ERROR_DMA;
  }
  const uint32_t known_errors =
    HAL_ADC_ERROR_INTERNAL |
    HAL_ADC_ERROR_OVR |
    HAL_ADC_ERROR_DMA;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_ADC_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 校验ADC实例是否已由板级配置启用。
 */
bool platform_adc_backend_is_valid(platform_adc_instance_t adc) {
  return (uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT &&
         platform_adc_to_hal(adc) != NULL;
}

/**
 * @brief 报告 ADC 后端是否支持独立 DMA 请求。
 */
bool platform_adc_backend_supports_dma(platform_adc_instance_t adc) {
  return platform_adc_backend_is_valid(adc);
}

/**
 * @brief 启动ADC后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_adc_backend_start(platform_adc_instance_t adc) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Start(adc_h));
}

/**
 * @brief 停止ADC后端并清理活动状态。
 */
platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop(adc_h));
}

/**
 * @brief 轮询 ADC 单次转换完成状态。
 */
platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_ADC_PollForConversion(adc_h, timeout)
                     );
}

/**
 * @brief 读取 ADC 转换码值。
 */
platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  if (adc_h == NULL || value == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  *value = HAL_ADC_GetValue(adc_h);
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动ADC中断转换。
 */
platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Start_IT(adc_h));
}

/**
 * @brief 停止ADC中断转换。
 */
platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop_IT(adc_h));
}

/**
 * @brief 启动ADC DMA 转换。
 */
platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_ADC_Start_DMA(adc_h, data, length)
                     );
}

/**
 * @brief 停止ADC DMA 转换。
 */
platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_ADC_Stop_DMA(adc_h));
}

/**
 * @brief 返回ADC后端记录的错误位。
 */
uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc) {
  ADC_HandleTypeDef *adc_h = platform_adc_to_hal(adc);
  return adc_h == NULL ? PLATFORM_ADC_ERROR_UNKNOWN
                   : platform_adc_map_error(HAL_ADC_GetError(adc_h));
}

/**
 * @brief 把 HAL ADC 回调事件转发给已注册的项目回调。
 */
static void platform_adc_dispatch_hal_event(
  ADC_HandleTypeDef *adc_h,
  platform_adc_event_type_t type
) {
  platform_adc_instance_t adc;
  if (platform_hal_to_adc(adc_h, &adc)) {
    platform_adc_dispatch_event(adc, type);
  }
}

/**
 * @brief 分发 ADC 转换完成事件并映射 HAL 状态。
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *adc_h) {
  platform_adc_dispatch_hal_event(adc_h, PLATFORM_ADC_EVENT_CONV_COMPLETE);
}

/**
 * @brief 分发 ADC 半转换完成事件并映射 HAL 状态。
 */
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *adc_h) {
  platform_adc_dispatch_hal_event(adc_h, PLATFORM_ADC_EVENT_CONV_HALF_COMPLETE);
}

/**
 * @brief 记录并分发 ADC 错误事件。
 */
void HAL_ADC_ErrorCallback(ADC_HandleTypeDef *adc_h) {
  platform_adc_dispatch_hal_event(adc_h, PLATFORM_ADC_EVENT_ERROR);
}
