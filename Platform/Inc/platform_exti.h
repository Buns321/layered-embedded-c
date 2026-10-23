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

#include <stdint.h>

#define TOTAL_EXTI_NUM 16

typedef void (*Platform_EXTI_Callback)(uint16_t GPIO_Pin, void* context);

/**
 * @brief EXTI 回调条目。
 */
typedef struct {
  Platform_EXTI_Callback exti_cb;
  void *context;
} platform_exti_callbacks_t;

/**
 * @brief 注册某个引脚的 EXTI 回调。
 * @param GPIO_Pin 单引脚掩码，必须是非零单 bit。
 * @param cb 回调条目，内容会被复制。
 * @note 回调在 EXTI 中断上下文执行，参数是触发中断的 GPIO Pin。
 */
void Platform_EXTI_RegisterCallbacks(uint16_t GPIO_Pin, const platform_exti_callbacks_t *cb);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_EXTI_H
