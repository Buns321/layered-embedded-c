/**
 * @file bsp_mcu.h
 * @brief 唯一芯片家族与主机测试桩头文件选择器。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_MCU_H
#define LAYERED_CAR_BSP_MCU_H

#if defined(BSP_HOST_STUB)
#include "bsp_host_types.h"
#elif defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#elif defined(STM32G4xx)
#include "stm32g4xx_hal.h"
#elif defined(STM32H7xx)
#include "stm32h7xx_hal.h"
#else
#error "Define STM32F4xx, STM32G4xx, STM32H7xx, or BSP_HOST_STUB"
#endif

/** @} */

#endif /* LAYERED_CAR_BSP_MCU_H */
