//
// Created by Buns_ on 2026/10/22.
//

/**
 * @file platform_config.h
 * @brief Compile-time availability of Platform peripheral instances.
 * @addtogroup platform
 * @{
 * @note CMake or the board configuration must define enabled instances.
 *       Disabled instances are omitted from the public identity enums.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CONFIG_H
#define LAYERED_EMBEDDED_C_PLATFORM_CONFIG_H

#ifndef PLATFORM_ENABLE_PWM_TIM1
#define PLATFORM_ENABLE_PWM_TIM1 0
#endif

#ifndef PLATFORM_ENABLE_PWM_TIM3
#define PLATFORM_ENABLE_PWM_TIM3 0
#endif

#ifndef PLATFORM_ENABLE_ENCODER_TIM1
#define PLATFORM_ENABLE_ENCODER_TIM1 0
#endif

#ifndef PLATFORM_ENABLE_ENCODER_TIM2
#define PLATFORM_ENABLE_ENCODER_TIM2 0
#endif

#ifndef PLATFORM_ENABLE_ENCODER_TIM3
#define PLATFORM_ENABLE_ENCODER_TIM3 0
#endif

#ifndef PLATFORM_ENABLE_ENCODER_TIM4
#define PLATFORM_ENABLE_ENCODER_TIM4 0
#endif

#ifndef PLATFORM_ENABLE_ENCODER_TIM5
#define PLATFORM_ENABLE_ENCODER_TIM5 0
#endif

#ifndef PLATFORM_ENABLE_UART1
#define PLATFORM_ENABLE_UART1 0
#endif

#ifndef PLATFORM_ENABLE_UART2
#define PLATFORM_ENABLE_UART2 0
#endif

#ifndef PLATFORM_ENABLE_UART3
#define PLATFORM_ENABLE_UART3 0
#endif

#ifndef PLATFORM_ENABLE_UART4
#define PLATFORM_ENABLE_UART4 0
#endif

#ifndef PLATFORM_ENABLE_UART5
#define PLATFORM_ENABLE_UART5 0
#endif

#ifndef PLATFORM_ENABLE_UART6
#define PLATFORM_ENABLE_UART6 0
#endif

#ifndef PLATFORM_ENABLE_CRC
#define PLATFORM_ENABLE_CRC 0
#endif

#ifndef PLATFORM_ENABLE_RNG
#define PLATFORM_ENABLE_RNG 0
#endif

#ifndef PLATFORM_ENABLE_IWDG
#define PLATFORM_ENABLE_IWDG 0
#endif

#ifndef PLATFORM_ENABLE_WWDG
#define PLATFORM_ENABLE_WWDG 0
#endif

#if PLATFORM_ENABLE_PWM_TIM1 && PLATFORM_ENABLE_ENCODER_TIM1
#error "TIM1 cannot be enabled as both PWM and encoder"
#endif

#if PLATFORM_ENABLE_PWM_TIM3 && PLATFORM_ENABLE_ENCODER_TIM3
#error "TIM3 cannot be enabled as both PWM and encoder"
#endif

/** @brief Number of possible UART identity slots on the supported MCU family. */
#define PLATFORM_UART_INSTANCE_COUNT 6u

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CONFIG_H
