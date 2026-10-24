/**
 * @file platform_exti_backend.h
 * @brief STM32 系列公共 EXTI 分发器的私有后端入口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_EXTI_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_EXTI_BACKEND_H

#include <stdint.h>

void platform_exti_dispatch_event(uint16_t GPIO_Pin);

#endif //LAYERED_EMBEDDED_C_PLATFORM_EXTI_BACKEND_H
