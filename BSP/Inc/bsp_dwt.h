/**
 * @file bsp_dwt.h
 * @brief 基于 DWT 的周期计数、微秒忙等与 RTOS/裸机毫秒延时。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_DWT_H
#define LAYERED_CAR_BSP_DWT_H

#include <stdint.h>

/**
 * @brief 使能 DWT->CYCCNT 周期计数器；运行期不要重复调用以免清零计数。
 */
void BSP_DWT_Init(void);

/**
 * @brief 读取当前 DWT 32 位周期计数值，单位 cycle。
 */
uint32_t BSP_DWT_GetCycles(void);

/**
 * @brief 将 32 位无符号周期差换算为微秒（向零截断），单位 us。
 */
uint32_t BSP_DWT_CyclesToUs(uint32_t cycles);

/**
 * @brief 忙等指定微秒数，单位 us；仅用于短时序，禁止长延时。
 */
void BSP_DWT_DelayUs(uint32_t us);

/**
 * @brief 延时指定毫秒数，单位 ms；FreeRTOS 运行时调用 osDelay，否则分片忙等。
 */
void BSP_DelayMs(uint32_t ms);

/** @} */

#endif /* LAYERED_CAR_BSP_DWT_H */
