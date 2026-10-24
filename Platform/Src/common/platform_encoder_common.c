/**
 * @file platform_encoder_common.c
 * @brief 与芯片系列无关的编码器参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_encoder_backend.h"
#include "platform_error_internal.h"

static volatile uint32_t encoder_error[6];

static uint32_t platform_encoder_error_index(
  platform_encoder_instance_t encoder
) {
  return (uint32_t)encoder;
}

static platform_status_t platform_encoder_finish(
  platform_encoder_instance_t encoder,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &encoder_error[platform_encoder_error_index(encoder)],
      platform_error_from_status(status)
    );
  }
  return status;
}

static platform_status_t platform_encoder_invalid(
  platform_encoder_instance_t encoder,
  bool instance_valid
) {
  if (instance_valid) {
    platform_error_record(
      &encoder_error[platform_encoder_error_index(encoder)],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

platform_status_t Platform_Encoder_Start(
  platform_encoder_instance_t encoder
) {
  const bool valid = platform_encoder_backend_is_valid(encoder);
  if (valid) {
    platform_error_reset(&encoder_error[platform_encoder_error_index(encoder)]);
  }
  if (!valid) {
    return platform_encoder_invalid(encoder, false);
  }
  return platform_encoder_finish(
    encoder,
    platform_encoder_backend_start(encoder)
  );
}

platform_status_t Platform_Encoder_Stop(
  platform_encoder_instance_t encoder
) {
  const bool valid = platform_encoder_backend_is_valid(encoder);
  if (valid) {
    platform_error_reset(&encoder_error[platform_encoder_error_index(encoder)]);
  }
  if (!valid) {
    return platform_encoder_invalid(encoder, false);
  }
  return platform_encoder_finish(
    encoder,
    platform_encoder_backend_stop(encoder)
  );
}

platform_status_t Platform_Encoder_GetCounter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
) {
  const bool valid = platform_encoder_backend_is_valid(encoder);
  if (valid) {
    platform_error_reset(&encoder_error[platform_encoder_error_index(encoder)]);
  }
  if (!valid || counter == NULL) {
    return platform_encoder_invalid(encoder, valid);
  }
  return platform_encoder_finish(
    encoder,
    platform_encoder_backend_get_counter(encoder, counter)
  );
}

platform_status_t Platform_Encoder_SetCounter(
  platform_encoder_instance_t encoder,
  uint32_t counter
) {
  const bool valid = platform_encoder_backend_is_valid(encoder);
  if (valid) {
    platform_error_reset(&encoder_error[platform_encoder_error_index(encoder)]);
  }
  if (!valid) {
    return platform_encoder_invalid(encoder, false);
  }
  return platform_encoder_finish(
    encoder,
    platform_encoder_backend_set_counter(encoder, counter)
  );
}

platform_status_t Platform_Encoder_GetCounterBits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
) {
  const bool valid = platform_encoder_backend_is_valid(encoder);
  if (valid) {
    platform_error_reset(&encoder_error[platform_encoder_error_index(encoder)]);
  }
  if (!valid || counter_bits == NULL) {
    return platform_encoder_invalid(encoder, valid);
  }
  return platform_encoder_finish(
    encoder,
    platform_encoder_backend_get_counter_bits(encoder, counter_bits)
  );
}

uint32_t Platform_Encoder_GetLastError(
  platform_encoder_instance_t encoder
) {
  if (!platform_encoder_backend_is_valid(encoder)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return encoder_error[platform_encoder_error_index(encoder)];
}
