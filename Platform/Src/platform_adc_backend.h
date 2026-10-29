/**
 * @file platform_adc_backend.h
 * @brief STM32 系列公共 ADC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H

#include "platform_adc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验ADC实例是否已由板级配置启用。
 */
bool platform_adc_backend_is_valid(platform_adc_instance_t adc);

/**
 * @brief 报告 ADC 后端是否支持独立 DMA 请求。
 */
bool platform_adc_backend_supports_dma(platform_adc_instance_t adc);

/**
 * @brief 启动ADC后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_adc_backend_start(platform_adc_instance_t adc);

/**
 * @brief 停止ADC后端并清理活动状态。
 */
platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc);

/**
 * @brief 轮询 ADC 单次转换完成状态。
 */
platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
);

/**
 * @brief 读取 ADC 转换码值。
 */
platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
);

/**
 * @brief 启动ADC中断转换。
 */
platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
);

/**
 * @brief 停止ADC中断转换。
 */
platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
);

/**
 * @brief 启动ADC DMA 转换。
 */
platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
);

/**
 * @brief 停止ADC DMA 转换。
 */
platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
);

/**
 * @brief 返回ADC后端记录的错误位。
 */
uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc);

/**
 * @brief 把 ADC 转换事件分发给已注册回调。
 */
void platform_adc_dispatch_event(
  platform_adc_instance_t adc,
  platform_adc_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
