/**
 * @file platform_encoder_common.c
 * @brief 与芯片系列无关的编码器参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_encoder_backend.h"
#include "platform_error_internal.h"

static volatile uint32_t encoder_error[6];

/**
 * @brief 把编码器实例编号转换为错误快照下标。
 */
static uint32_t platform_encoder_error_index(
  platform_encoder_instance_t encoder
) {
  return (uint32_t)encoder;
}

/**
 * @brief 把 HAL 编码器结果转换为公共状态并记录错误。
 */
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

/**
 * @brief 构造编码器非法参数状态。
 */
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

/**
 * @brief 启动编码器接口。
 * @param encoder 编码器定时器身份。
 * @retval PLATFORM_STATUS_OK 成功。
 * @retval PLATFORM_STATUS_ERROR 失败。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 Encoder 模式。
 * @note 编码器模式固定使用定时器的两个输入通道。
 */
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

/** @brief 停止编码器接口。 */
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

/**
 * @brief 读取硬件计数器。
 * @param encoder 编码器定时器身份。
 * @param counter 当前计数值输出。
 */
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

/**
 * @brief 写入硬件计数器。
 * @param encoder 编码器定时器身份。
 * @param counter 目标计数值。
 */
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

/**
 * @brief 查询定时器计数位宽。
 * @param encoder 编码器定时器身份。
 * @param counter_bits 位宽输出，值为 16 或 32。
 */
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

/** @brief 读取最近一次编码器操作错误快照。 */
uint32_t Platform_Encoder_GetLastError(
  platform_encoder_instance_t encoder
) {
  if (!platform_encoder_backend_is_valid(encoder)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return encoder_error[platform_encoder_error_index(encoder)];
}
