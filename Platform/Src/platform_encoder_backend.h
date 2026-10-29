/**
 * @file platform_encoder_backend.h
 * @brief STM32 系列公共 encoder 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H

#include "platform_def.h"
#include "platform_encoder.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 校验编码器实例是否已由板级配置启用。
 */
bool platform_encoder_backend_is_valid(platform_encoder_instance_t encoder);

/**
 * @brief 启动编码器后端，不负责 CubeMX 结构性初始化。
 */
platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
);

/**
 * @brief 停止编码器后端并清理活动状态。
 */
platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
);

/**
 * @brief 读取编码器硬件计数器当前值。
 */
platform_status_t platform_encoder_backend_get_counter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
);

/**
 * @brief 写入编码器硬件计数器。
 */
platform_status_t platform_encoder_backend_set_counter(
  platform_encoder_instance_t encoder,
  uint32_t counter
);

/**
 * @brief 返回编码器计数器位宽。
 */
platform_status_t platform_encoder_backend_get_counter_bits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
