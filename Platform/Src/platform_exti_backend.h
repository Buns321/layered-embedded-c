/**
 * @file platform_exti_backend.h
 * @brief STM32 系列公共 EXTI 分发器的私有后端入口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_EXTI_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_EXTI_BACKEND_H

#include <stdint.h>

/**
 * @brief 把 EXTI 引脚事件分发给已注册回调。
 * @param GPIO_Pin 触发中断的单 bit 引脚掩码。
 * @post 对应线路已注册且回调非空时调用一次回调；非法、未注册或空回调时静默返回。
 * @note 仅供 HAL EXTI 回调在 ISR 上下文调用；不得阻塞或执行长耗时解析。
 * @note 注册状态在运行期不修改，本函数不分配内存。
 */
void platform_exti_dispatch_event(uint16_t GPIO_Pin);

#endif //LAYERED_EMBEDDED_C_PLATFORM_EXTI_BACKEND_H
