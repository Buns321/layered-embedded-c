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
typedef enum {
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
} Platform_Encoder_Instance;

#if !PLATFORM_ENABLE_ENCODER_TIM1 && !PLATFORM_ENABLE_ENCODER_TIM2 && \
    !PLATFORM_ENABLE_ENCODER_TIM3 && !PLATFORM_ENABLE_ENCODER_TIM4 && \
    !PLATFORM_ENABLE_ENCODER_TIM5
#error "At least one Platform encoder instance must be enabled"
#endif

/**
 * @brief 启动编码器接口。
 * @param encoder 编码器定时器身份。
 * @retval Platform_OK 成功。
 * @retval Platform_ERROR 失败。
 * @pre 对应定时器的 MX_TIMx_Init() 已完成并配置为 Encoder 模式。
 * @note 编码器模式固定使用定时器的两个输入通道。
 */
Platform_StatusTypeDef Platform_Encoder_Start(
  Platform_Encoder_Instance encoder
);

/**
 * @brief 读取硬件计数器。
 * @param encoder 编码器定时器身份。
 * @return 当前计数值。
 */
uint32_t Platform_Encoder_GetCounter(
  Platform_Encoder_Instance encoder
);

/**
 * @brief 写入硬件计数器。
 * @param encoder 编码器定时器身份。
 * @param counter 目标计数值。
 */
void Platform_Encoder_SetCounter(
  Platform_Encoder_Instance encoder,
  uint32_t counter
);

/**
 * @brief 查询定时器计数位宽。
 * @param encoder 编码器定时器身份。
 * @return 16 或 32。
 */
uint8_t Platform_Encoder_GetCounterBits(
  Platform_Encoder_Instance encoder
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_H
