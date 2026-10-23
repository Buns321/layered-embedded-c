//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_dac.h
 * @brief 不依赖 HAL 类型的 DAC 输出接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_DAC1
#error "Platform DAC1 must be enabled before including platform_dac.h"
#endif

/** @brief 可用的 DAC 实例。 */
typedef enum platform_dac_instance_e {
  PLATFORM_DAC_1 = 0U,
} platform_dac_instance_t;

/** @brief DAC 身份槽位数量上限。 */
#define PLATFORM_DAC_INSTANCE_COUNT 1U

/** @brief DAC 输出通道。 */
typedef enum platform_dac_channel_e {
  PLATFORM_DAC_CHANNEL_1 = 0U,
  PLATFORM_DAC_CHANNEL_2 = 1U,
} platform_dac_channel_t;

/** @brief 已配置 DAC 通道使用的数值对齐方式。 */
typedef enum platform_dac_alignment_e {
  PLATFORM_DAC_ALIGN_8_BIT_RIGHT = 0U,
  PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
  PLATFORM_DAC_ALIGN_12_BIT_LEFT,
} platform_dac_alignment_t;

/** @brief DAC 错误标志。 */
typedef enum platform_dac_error_e {
  PLATFORM_DAC_ERROR_NONE          = 0u,
  PLATFORM_DAC_ERROR_DMA_UNDERRUN  = (1u << 0),
  PLATFORM_DAC_ERROR_DMA           = (1u << 1),
  PLATFORM_DAC_ERROR_TIMEOUT       = (1u << 2),
  PLATFORM_DAC_ERROR_UNKNOWN       = (1u << 7),
} platform_dac_error_t;

/** @brief DAC 异步事件。 */
typedef enum platform_dac_event_type_e {
  PLATFORM_DAC_EVENT_CONV_COMPLETE = 0U,
  PLATFORM_DAC_EVENT_CONV_HALF_COMPLETE,
  PLATFORM_DAC_EVENT_ERROR,
  PLATFORM_DAC_EVENT_UNDERRUN,
} platform_dac_event_type_t;

/** @brief DAC 事件数据，只在回调期间有效。 */
typedef struct platform_dac_event_s {
  platform_dac_event_type_t type;
  platform_dac_channel_t channel;
  uint32_t error;
} platform_dac_event_t;

typedef void (*platform_dac_callback_t)(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event,
  void *context
);

/**
 * @brief DAC 转换回调集合。
 * @note 回调在中断上下文执行。
 * @note 一个物理 DAC 实例只能有一个回调所有者。
 */
typedef struct platform_dac_callback_binding_s {
  platform_dac_callback_t callback;
  void *context;
} platform_dac_callback_binding_t;

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
);

/**
 * @brief 按 CubeMX 配置的触发方式启动 DAC 通道。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 */
platform_status_t Platform_DAC_Start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

/**
 * @brief 停止 DAC 通道。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 */
platform_status_t Platform_DAC_Stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

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
);

/**
 * @brief 停止 DAC 通道的 DMA 输出。
 * @param dac DAC 实例。
 * @param channel DAC 输出通道。
 */
platform_status_t Platform_DAC_StopDMA(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
);

/**
 * @brief 读取最近一次 HAL DAC 错误快照。
 * @param dac DAC 实例。
 */
uint32_t Platform_DAC_GetLastError(platform_dac_instance_t dac);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_H
