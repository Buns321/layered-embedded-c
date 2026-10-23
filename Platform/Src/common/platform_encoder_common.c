/**
 * @file platform_encoder_common.c
 * @brief Family-independent encoder validation and error snapshot.
 */

#include "platform_encoder_backend.h"
#include "platform_error_internal.h"

static volatile uint32_t encoder_error[6];

static uint32_t platform_encoder_error_index(
  Platform_Encoder_Instance encoder
) {
  return (uint32_t)encoder;
}

static Platform_StatusTypeDef platform_encoder_finish(
  Platform_Encoder_Instance encoder,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &encoder_error[platform_encoder_error_index(encoder)],
      platform_error_from_status(status)
    );
  }
  return status;
}

static Platform_StatusTypeDef platform_encoder_invalid(
  Platform_Encoder_Instance encoder,
  bool instance_valid
) {
  if (instance_valid) {
    platform_error_record(
      &encoder_error[platform_encoder_error_index(encoder)],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

Platform_StatusTypeDef Platform_Encoder_Start(
  Platform_Encoder_Instance encoder
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

Platform_StatusTypeDef Platform_Encoder_Stop(
  Platform_Encoder_Instance encoder
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

Platform_StatusTypeDef Platform_Encoder_GetCounter(
  Platform_Encoder_Instance encoder,
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

Platform_StatusTypeDef Platform_Encoder_SetCounter(
  Platform_Encoder_Instance encoder,
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

Platform_StatusTypeDef Platform_Encoder_GetCounterBits(
  Platform_Encoder_Instance encoder,
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
  Platform_Encoder_Instance encoder
) {
  if (!platform_encoder_backend_is_valid(encoder)) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return encoder_error[platform_encoder_error_index(encoder)];
}
