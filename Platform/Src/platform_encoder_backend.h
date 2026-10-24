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

bool platform_encoder_backend_is_valid(platform_encoder_instance_t encoder);

platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
);

platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
);

platform_status_t platform_encoder_backend_get_counter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
);

platform_status_t platform_encoder_backend_set_counter(
  platform_encoder_instance_t encoder,
  uint32_t counter
);

platform_status_t platform_encoder_backend_get_counter_bits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
