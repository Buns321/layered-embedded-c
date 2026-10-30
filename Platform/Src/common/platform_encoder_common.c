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
 * @param encoder 编码器定时器身份。
 * @return 与枚举值相同的快照下标。
 * @pre 调用方必须先通过 platform_encoder_backend_is_valid() 确认实例有效。
 * @note 本函数不检查范围；非法值会形成越界下标，仅供合法路径使用。
 */
static uint32_t platform_encoder_error_index(
  platform_encoder_instance_t encoder
) {
  return (uint32_t)encoder;
}

/**
 * @brief 把编码器后端结果转换为公共状态并保留失败快照。
 * @param encoder 已确认有效的编码器定时器身份。
 * @param status 后端返回的平台状态。
 * @return 原样返回 status，便于调用方直接返回。
 * @post status 非 OK 时把对应通用错误位合并到该实例快照；OK 不修改快照。
 * @note 仅供本文件公共操作收尾使用；不访问硬件，不分配内存。
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
 * @brief 记录编码器参数错误并构造统一失败状态。
 * @param encoder 编码器定时器身份。
 * @param instance_valid true 表示 encoder 可安全索引错误快照数组。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post instance_valid 为 true 时记录 PLATFORM_ERROR_INVALID_ARGUMENT；
 *       否则不访问快照数组。
 * @note 仅供本文件失败路径使用；不访问编码器硬件，不分配内存。
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
 * @param encoder 已由 CubeMX 配置为 Encoder 模式的定时器身份。
 * @retval PLATFORM_STATUS_OK 两个编码器输入通道已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用或非法。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 Encoder 模式。
 * @note 编码器模式固定使用定时器的两个输入通道。
 * @note 同一实例的启动、停止和计数器写入须由调用方串行化；不分配内存。
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

/**
 * @brief 停止编码器接口。
 * @param encoder 已启用的编码器定时器身份。
 * @retval PLATFORM_STATUS_OK 编码器输入通道已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用或非法。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败。
 * @post 计数器值保持不变，重新启动后继续计数。
 * @note 不分配内存；同一实例操作须由调用方串行化。
 */
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
 * @param encoder 已启用的编码器定时器身份。
 * @param counter 当前计数值输出地址，单位为编码器计数 counts；返回原值不换算转速。
 * @retval PLATFORM_STATUS_OK counter 已写入当前硬件计数值。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 counter 为空。
 * @retval PLATFORM_STATUS_ERROR HAL 读取失败。
 * @note counter 只在调用期间需要有效，所有权仍归调用方；不分配内存。
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
 * @param encoder 已启用的编码器定时器身份。
 * @param counter 目标计数值，单位为编码器计数 counts；超过定时器位宽时由硬件截断。
 * @retval PLATFORM_STATUS_OK 计数器已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例未启用或非法。
 * @retval PLATFORM_STATUS_ERROR HAL 写入失败。
 * @note 写入会改变后续相对计数的基准；同一实例读写须由调用方串行化。
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
 * @param encoder 已启用的编码器定时器身份。
 * @param counter_bits 位宽输出地址；成功时写入 16 或 32。
 * @retval PLATFORM_STATUS_OK 位宽已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 counter_bits 为空。
 * @retval PLATFORM_STATUS_ERROR 后端无法描述位宽或查询失败。
 * @note counter_bits 只需在调用期间有效；不分配内存。
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

/**
 * @brief 读取最近一次编码器操作错误快照。
 * @param encoder 编码器定时器身份。
 * @return 该实例的 PLATFORM_ERROR_* 位掩码；非法实例返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除快照；后续公共编码器操作开始时会清除对应快照。
 * @note 不访问硬件更新错误；并发读写同一快照须由调用方串行化。
 */
uint32_t Platform_Encoder_GetLastError(
  platform_encoder_instance_t encoder
) {
  if (!platform_encoder_backend_is_valid(encoder)) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return encoder_error[platform_encoder_error_index(encoder)];
}
