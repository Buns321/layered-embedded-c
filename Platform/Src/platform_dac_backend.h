/**
 * @file platform_dac_backend.h
 * @brief STM32 系列公共 DAC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H

#include "platform_dac.h"
#include "platform_def.h"
#include <stdbool.h>

/**
 * @brief 校验DAC实例是否已由板级配置启用。
 */
bool platform_dac_backend_is_valid(platform_dac_instance_t dac);

/**
 * @brief 写入 DAC 原始码值。
 */
platform_status_t platform_dac_backend_set_value(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
);

/**
 * @brief 启动DAC后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_dac_backend_start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

/**
 * @brief 停止DAC后端并清理活动状态。
 */
platform_status_t platform_dac_backend_stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

/**
 * @brief 启动DAC DMA 转换。
 */
platform_status_t platform_dac_backend_start_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
);

/**
 * @brief 停止DAC DMA 转换。
 */
platform_status_t platform_dac_backend_stop_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

/**
 * @brief 返回DAC后端记录的错误位。
 */
uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac);

/**
 * @brief 把 DAC 转换事件分发给已注册回调。
 */
void platform_dac_dispatch_event(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
