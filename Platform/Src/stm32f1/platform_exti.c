/**
 * @file platform_exti.c
 * @brief STM32F1 公共 EXTI 分发器的 HAL 入口。
 * @ingroup platform
 */

#include "platform_exti_backend.h"
#include "stm32f1xx_hal.h"

/**
 * @brief 把 STM32F1 HAL EXTI 中断转发给平台分发器。
 * @param GPIO_Pin 触发中断的单 bit 引脚掩码；由 HAL 根据 EXTI 挂起位生成。
 * @post 已注册回调在返回前同步执行一次；非法引脚、未注册线路或空回调时静默返回。
 * @warning 由 HAL 中断上下文调用；不得阻塞、分配内存或在回调中执行长耗时解析。
 * @note 本函数不修改注册表或 GPIO，事件仅代表本次 EXTI 边沿。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  platform_exti_dispatch_event(GPIO_Pin);
}
