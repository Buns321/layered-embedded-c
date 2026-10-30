//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file platform_encoder.h
 * @brief 编码器定时器抽象：启动、计数器读写与位宽查询。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
#define LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

/**
 * @brief 可用的硬件编码器定时器身份。
 * @note 未启用的实例不会进入该枚举。
 */
typedef enum platform_encoder_instance_e {
#if PLATFORM_ENABLE_ENCODER_TIM1
  PLATFORM_ENCODER_TIM1 = 1,
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
  PLATFORM_ENCODER_TIM2 = 2,
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
  PLATFORM_ENCODER_TIM3 = 3,
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
  PLATFORM_ENCODER_TIM4 = 4,
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
  PLATFORM_ENCODER_TIM5 = 5,
#endif
} platform_encoder_instance_t;

#if !PLATFORM_ENABLE_ENCODER_TIM1 && !PLATFORM_ENABLE_ENCODER_TIM2 && \
    !PLATFORM_ENABLE_ENCODER_TIM3 && !PLATFORM_ENABLE_ENCODER_TIM4 && \
    !PLATFORM_ENABLE_ENCODER_TIM5
#error "At least one Platform encoder instance must be enabled"
#endif

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
);

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
);

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
);

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
);

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
);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
