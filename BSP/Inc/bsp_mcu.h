/**
 * @file bsp_mcu.h
 * @brief 唯一芯片家族与主机测试桩头文件选择器。
 * @details 集中隔离 STM32F4/G4/H7 HAL 头文件与主机单元测试桩（BSP_HOST_STUB）的条件包含，
 *          确保 BSP 其余模块无需重复编写芯片家族预处理分支。
 *          本文件仅允许被 BSP 与 User/board_cfg 等底层模块直接或间接包含，严禁渗透至应用视图。
 *
 * @addtogroup BSP
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

#endif /* LAYERED_CAR_BSP_MCU_H */

/** @} */
