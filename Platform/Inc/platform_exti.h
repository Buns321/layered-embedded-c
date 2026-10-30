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
 * @param GPIO_Pin 非零单 bit 引脚掩码，对应 EXTI 线 0..15。
 * @param cb 回调条目；函数复制结构和函数指针，不接管 context 的所有权。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该 EXTI 线已注册。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT GPIO_Pin 为非零单 bit 之外的掩码，cb 为空，
 *         或映射结果超出 16 条 EXTI 线。
 * @pre 应在对应 EXTI 线和 NVIC 使能前完成注册。
 * @note 回调在 EXTI 中断上下文执行，参数是触发中断的 GPIO_Pin；不得阻塞。
 * @note 一个 EXTI 线只允许一个回调所有者，不支持静默覆盖。
 * @note cb 只需在调用期间有效，context 生命周期由调用方保证；不分配内存。
 */
platform_status_t Platform_EXTI_RegisterCallback(
  uint16_t GPIO_Pin,
  const platform_exti_callback_binding_t *cb
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_EXTI_H
