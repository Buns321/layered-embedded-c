/**
 * @file platform_adc_common.c
 * @brief 与芯片系列无关的 ADC 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_adc_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t adc_error[PLATFORM_ADC_INSTANCE_COUNT];
static platform_adc_callback_binding_t adc_cb[PLATFORM_ADC_INSTANCE_COUNT];
static bool adc_cb_registered[PLATFORM_ADC_INSTANCE_COUNT];

/**
 * @brief 把 HAL ADC 结果转换为公共状态并记录错误。
 */
static platform_status_t platform_adc_finish(
  platform_adc_instance_t adc,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &adc_error[adc],
      platform_error_from_status(status)
    );
  }
  return status;
}

/**
 * @brief 构造 ADC 非法参数状态。
 */
static platform_status_t platform_adc_invalid(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 把 ADC 转换事件分发给已注册回调。
 */
void platform_adc_dispatch_event(
  platform_adc_instance_t adc,
  platform_adc_event_type_t type
) {
  if (
    (uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT ||
    !adc_cb_registered[adc]
  ) {
    return;
  }

  platform_adc_event_t event = {
    .type = type,
    .error = type == PLATFORM_ADC_EVENT_ERROR
               ? platform_adc_backend_get_error(adc)
               : PLATFORM_ADC_ERROR_NONE,
  };
  if (adc_cb[adc].callback != NULL) {
    adc_cb[adc].callback(adc, &event, adc_cb[adc].context);
  }
}

/**
 * @brief 以轮询模式启动转换。
 * @param adc ADC 实例。
 * @pre MX_ADCx_Init() 已完成，且规则组通道已配置。
 */
platform_status_t Platform_ADC_Start(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_start(adc));
}

/**
 * @brief 停止轮询转换。
 * @param adc ADC 实例。
 */
platform_status_t Platform_ADC_Stop(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_stop(adc));
}

/**
 * @brief 等待当前轮询转换完成。
 * @param adc ADC 实例。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 */
platform_status_t Platform_ADC_PollForConversion(
  platform_adc_instance_t adc,
  uint32_t timeout
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_poll(adc, timeout));
}

/**
 * @brief 读取最近一次规则组转换结果。
 * @param adc ADC 实例。
 * @param value 转换结果输出。
 * @note 除非 CubeMX 另行配置，该值是原始 ADC 码值。
 */
platform_status_t Platform_ADC_GetValue(
  platform_adc_instance_t adc,
  uint32_t *value
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc) || value == NULL) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(
    adc,
    platform_adc_backend_get_value(adc, value)
  );
}

/**
 * @brief 执行一次阻塞式规则组转换序列。
 * @param adc ADC 实例。
 * @param value 最近一次原始转换结果的目标地址。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 * @pre CubeMX 应配置为单通道、单次转换的规则组。
 * @note 多通道扫描模式应使用 DMA。规则组中断只会报告扫描序列的最后一次
 *       转换结果。
 * @note 返回前会停止转换。
 */
platform_status_t Platform_ADC_Read(
  platform_adc_instance_t adc,
  uint32_t *value,
  uint32_t timeout
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc) || value == NULL) {
    return platform_adc_invalid(adc);
  }

  platform_status_t status = platform_adc_backend_start(adc);
  if (status != PLATFORM_STATUS_OK) {
    return platform_adc_finish(adc, status);
  }

  status = platform_adc_backend_poll(adc, timeout);
  if (status == PLATFORM_STATUS_OK) {
    status = platform_adc_backend_get_value(adc, value);
  }
  const platform_status_t stop_status =
    platform_adc_backend_stop(adc);
  if (status == PLATFORM_STATUS_OK) {
    status = stop_status;
  }
  return platform_adc_finish(adc, status);
}

/**
 * @brief 使用规则组中断启动转换。
 * @param adc ADC 实例。
 */
platform_status_t Platform_ADC_StartIT(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_start_it(adc));
}

/**
 * @brief 停止中断方式转换。
 * @param adc ADC 实例。
 */
platform_status_t Platform_ADC_StopIT(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  return platform_adc_finish(adc, platform_adc_backend_stop_it(adc));
}

/**
 * @brief 启动规则组 DMA 转换。
 * @param adc ADC 实例。
 * @param data uint32_t 转换结果的目标数组。
 * @param length 转换次数，不是字节数。
 * @note 在完成、错误或停止前，data 必须保持有效。
 * @note STM32F1 上只有 ADC1 具有 DMA 请求。对不支持的实例启动或停止 DMA
 *       会返回 PLATFORM_STATUS_ERROR。
 */
platform_status_t Platform_ADC_StartDMA(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (
    !platform_adc_backend_is_valid(adc) ||
    data == NULL ||
    length == 0U
  ) {
    return platform_adc_invalid(adc);
  }
  if (!platform_adc_backend_supports_dma(adc)) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_ERROR_UNSUPPORTED
    );
    return PLATFORM_STATUS_UNSUPPORTED;
  }
  return platform_adc_finish(
    adc,
    platform_adc_backend_start_dma(adc, data, length)
  );
}

/**
 * @brief 停止 DMA 转换。
 * @param adc ADC 实例。
 * @note STM32F1 上只支持停止 ADC1。
 */
platform_status_t Platform_ADC_StopDMA(platform_adc_instance_t adc) {
  if ((uint32_t)adc < PLATFORM_ADC_INSTANCE_COUNT) {
    platform_error_reset(&adc_error[adc]);
  }
  if (!platform_adc_backend_is_valid(adc)) {
    return platform_adc_invalid(adc);
  }
  if (!platform_adc_backend_supports_dma(adc)) {
    platform_error_record(
      &adc_error[adc],
      PLATFORM_ERROR_UNSUPPORTED
    );
    return PLATFORM_STATUS_UNSUPPORTED;
  }
  return platform_adc_finish(adc, platform_adc_backend_stop_dma(adc));
}

/**
 * @brief 读取最近一次 HAL ADC 错误快照。
 * @param adc ADC 实例。
 */
uint32_t Platform_ADC_GetLastError(platform_adc_instance_t adc) {
  if ((uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return adc_error[adc] | platform_adc_backend_get_error(adc);
}

/**
 * @brief 注册 ADC 回调。
 * @param adc ADC 实例。
 * @param callbacks 待复制的回调集合。
 * @note 应在初始化阶段、启动 IT/DMA 转换前调用一次。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 */
platform_status_t Platform_ADC_RegisterCallback(
  platform_adc_instance_t adc,
  const platform_adc_callback_binding_t *callbacks
) {
  if (
    (uint32_t)adc >= PLATFORM_ADC_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_adc_backend_is_valid(adc)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (adc_cb_registered[adc]) {
    return PLATFORM_STATUS_BUSY;
  }

  adc_cb[adc] = *callbacks;
  adc_cb_registered[adc] = true;
  return PLATFORM_STATUS_OK;
}
