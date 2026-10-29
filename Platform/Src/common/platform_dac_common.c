/**
 * @file platform_dac_common.c
 * @brief 与芯片系列无关的 DAC 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_dac_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t dac_error[PLATFORM_DAC_INSTANCE_COUNT];
static platform_dac_callback_binding_t dac_cb[PLATFORM_DAC_INSTANCE_COUNT];
static bool dac_cb_registered[PLATFORM_DAC_INSTANCE_COUNT];

/**
 * @brief 把 HAL DAC 结果转换为公共状态并记录错误。
 */
static platform_status_t platform_dac_finish(
  platform_dac_instance_t dac,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &dac_error[dac],
      platform_error_from_status(status)
    );
  }
  return status;
}

/**
 * @brief 构造 DAC 非法参数状态。
 */
static platform_status_t platform_dac_invalid(platform_dac_instance_t dac) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_record(
      &dac_error[dac],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 校验 DAC 通道枚举。
 */
static bool platform_dac_channel_valid(platform_dac_channel_t channel) {
  return channel == PLATFORM_DAC_CHANNEL_1 ||
         channel == PLATFORM_DAC_CHANNEL_2;
}

/**
 * @brief 校验 DAC 数据对齐枚举。
 */
static bool platform_dac_alignment_valid(platform_dac_alignment_t alignment) {
  return alignment == PLATFORM_DAC_ALIGN_8_BIT_RIGHT ||
         alignment == PLATFORM_DAC_ALIGN_12_BIT_RIGHT ||
         alignment == PLATFORM_DAC_ALIGN_12_BIT_LEFT;
}

/**
 * @brief 校验 DAC 输出值范围。
 */
static bool platform_dac_value_valid(
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  return alignment == PLATFORM_DAC_ALIGN_8_BIT_RIGHT
           ? value <= 0xFFU
           : value <= 0xFFFU;
}

/**
 * @brief 把 DAC 转换事件分发给已注册回调。
 */
void platform_dac_dispatch_event(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event
) {
  if (
    (uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT ||
    event == NULL ||
    !dac_cb_registered[dac]
  ) {
    return;
  }
  if (dac_cb[dac].callback != NULL) {
    dac_cb[dac].callback(dac, event, dac_cb[dac].context);
  }
}

/**
 * @brief 设置 DAC 输出寄存器值，但不启动通道。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 * @param alignment 数值对齐方式。
 * @param value 原始码值，其位宽由 alignment 描述。
 */
platform_status_t Platform_DAC_SetValue(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel) ||
    !platform_dac_alignment_valid(alignment) ||
    !platform_dac_value_valid(alignment, value)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_set_value(dac, channel, alignment, value)
  );
}

/**
 * @brief 按 CubeMX 配置的触发方式启动 DAC 通道。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 */
platform_status_t Platform_DAC_Start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_start(dac, channel)
  );
}

/**
 * @brief 停止 DAC 通道。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 */
platform_status_t Platform_DAC_Stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_stop(dac, channel)
  );
}

/**
 * @brief 启动 DAC 通道的 DMA 输出。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 * @param alignment data 中每个样本的对齐方式。
 * @param data 原始 uint32_t 样本数组。样本对齐方式必须与 CubeMX 中的 DAC
 *             通道配置一致。
 * @param length 样本数，不是字节数。
 * @note 在完成、错误或停止前，data 必须保持有效。
 */
platform_status_t Platform_DAC_StartDMA(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel) ||
    !platform_dac_alignment_valid(alignment) ||
    data == NULL ||
    length == 0U
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_start_dma(
      dac,
      channel,
      alignment,
      data,
      length
    )
  );
}

/**
 * @brief 停止 DAC 通道的 DMA 输出。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 */
platform_status_t Platform_DAC_StopDMA(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_stop_dma(dac, channel)
  );
}

/**
 * @brief 读取最近一次 HAL DAC 错误快照。
 * @param dac DAC 实例。
 */
uint32_t Platform_DAC_GetLastError(platform_dac_instance_t dac) {
  if ((uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return dac_error[dac] | platform_dac_backend_get_error(dac);
}

/**
 * @brief 注册 DAC 回调。
 * @param dac DAC 实例。
 * @param callbacks 待复制的回调集合。
 * @note 应在初始化阶段、启动 DMA 输出前调用一次。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 */
platform_status_t Platform_DAC_RegisterCallback(
  platform_dac_instance_t dac,
  const platform_dac_callback_binding_t *callbacks
) {
  if (
    (uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_dac_backend_is_valid(dac)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (dac_cb_registered[dac]) {
    return PLATFORM_STATUS_BUSY;
  }

  dac_cb[dac] = *callbacks;
  dac_cb_registered[dac] = true;
  return PLATFORM_STATUS_OK;
}
