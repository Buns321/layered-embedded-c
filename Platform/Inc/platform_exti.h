//
// 由 Buns_ 创建于 2026/8/27。
//

/**
 * @file platform_exti.h
 * @brief EXTI 回调注册与分发。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_EXTI_H
#define LAYERED_EMBEDDED_C_PLATFORM_EXTI_H

#include "platform_def.h"
#include <stdint.h>

#define PLATFORM_EXTI_LINE_COUNT 16u
#define TOTAL_EXTI_NUM PLATFORM_EXTI_LINE_COUNT

typedef void (*platform_exti_callback_t)(uint16_t GPIO_Pin, void* context);

/**
 * @brief EXTI 回调条目。
 */
typedef struct platform_exti_callback_binding_s {
  platform_exti_callback_t exti_cb;
  void *context;
} platform_exti_callback_binding_t;

/**
 * @brief 注册某个引脚的 EXTI 回调。
 * @param GPIO_Pin 单引脚掩码，必须是非零单 bit。
 * @param cb 回调条目，内容会被复制。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 EXTI 线已注册。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 引脚或回调条目非法。
 * @note 回调在 EXTI 中断上下文执行，参数是触发中断的 GPIO Pin。
 * @note 一个 EXTI 线只允许一个回调所有者，不支持静默覆盖。
 */
platform_status_t Platform_EXTI_RegisterCallback(
  uint16_t GPIO_Pin,
  const platform_exti_callback_binding_t *cb
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_EXTI_H
