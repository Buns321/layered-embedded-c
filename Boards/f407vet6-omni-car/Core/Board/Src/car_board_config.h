/**
 * @file car_board_config.h
 * @brief F407 四轮小车私有硬件映射。
 * @ingroup core_board
 */

#ifndef LAYERED_EMBEDDED_C_CAR_BOARD_CONFIG_H
#define LAYERED_EMBEDDED_C_CAR_BOARD_CONFIG_H

#include "platform_gpio.h"
#include "platform_pwm.h"
#include <stdint.h>

/** @brief 四轮 PWM 定时器。 */
#define CAR_BOARD_PWM_INSTANCE PLATFORM_PWM_TIM1

/** @brief PWM duty 量程；当前 TIM1 ARR 为 999。 */
#define CAR_BOARD_PWM_MAX_DUTY 1000

/** @brief 编码器每米计数，包含减速比和轮径。 */
#define CAR_BOARD_COUNTS_PER_METER 1000.0f

/** @brief 四轮中心到车体中心的距离，单位 m。 */
#define CAR_BOARD_WHEEL_DISTANCE_M 0.12f

/** @brief AB 组 STBY 端口。 */
#define CAR_BOARD_STBY_AB_PORT PLATFORM_GPIO_PORT_C
/** @brief AB 组 STBY 引脚。 */
#define CAR_BOARD_STBY_AB_PIN ((uint16_t)0x2000u)

/** @brief CD 组 STBY 端口。 */
#define CAR_BOARD_STBY_CD_PORT PLATFORM_GPIO_PORT_E
/** @brief CD 组 STBY 引脚。 */
#define CAR_BOARD_STBY_CD_PIN ((uint16_t)0x0020u)

/** @brief A 轮 AIN1 端口。 */
#define CAR_BOARD_AIN1_A_PORT PLATFORM_GPIO_PORT_D
/** @brief A 轮 AIN1 引脚。 */
#define CAR_BOARD_AIN1_A_PIN ((uint16_t)0x0001u)
/** @brief A 轮 AIN2 端口。 */
#define CAR_BOARD_AIN2_A_PORT PLATFORM_GPIO_PORT_D
/** @brief A 轮 AIN2 引脚。 */
#define CAR_BOARD_AIN2_A_PIN ((uint16_t)0x0002u)

/** @brief B 轮 AIN1 端口。 */
#define CAR_BOARD_AIN1_B_PORT PLATFORM_GPIO_PORT_D
/** @brief B 轮 AIN1 引脚。 */
#define CAR_BOARD_AIN1_B_PIN ((uint16_t)0x0004u)
/** @brief B 轮 AIN2 端口。 */
#define CAR_BOARD_AIN2_B_PORT PLATFORM_GPIO_PORT_D
/** @brief B 轮 AIN2 引脚。 */
#define CAR_BOARD_AIN2_B_PIN ((uint16_t)0x0008u)

/** @brief C 轮 AIN1 端口。 */
#define CAR_BOARD_AIN1_C_PORT PLATFORM_GPIO_PORT_D
/** @brief C 轮 AIN1 引脚。 */
#define CAR_BOARD_AIN1_C_PIN ((uint16_t)0x0010u)
/** @brief C 轮 AIN2 端口。 */
#define CAR_BOARD_AIN2_C_PORT PLATFORM_GPIO_PORT_D
/** @brief C 轮 AIN2 引脚。 */
#define CAR_BOARD_AIN2_C_PIN ((uint16_t)0x0020u)

/** @brief D 轮 AIN1 端口。 */
#define CAR_BOARD_AIN1_D_PORT PLATFORM_GPIO_PORT_D
/** @brief D 轮 AIN1 引脚。 */
#define CAR_BOARD_AIN1_D_PIN ((uint16_t)0x0040u)
/** @brief D 轮 AIN2 端口。 */
#define CAR_BOARD_AIN2_D_PORT PLATFORM_GPIO_PORT_D
/** @brief D 轮 AIN2 引脚。 */
#define CAR_BOARD_AIN2_D_PIN ((uint16_t)0x0080u)

#endif // LAYERED_EMBEDDED_C_CAR_BOARD_CONFIG_H
