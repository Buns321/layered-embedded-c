/**
 * @file bsp_dwt.h
 * @brief 基于 Cortex-M DWT 的周期计数、微秒忙等与 RTOS/裸机毫秒延时接口。
 * @details 利用 CoreDebug 与 DWT->CYCCNT 提供高分辨率时间戳和短时序忙等延时，
 *          并在启用 FreeRTOS 且调度器运行时自动将毫秒延时让渡给 osDelay()。
 *          本模块无内部动态内存分配，不依赖外设中断。
 *
 * @addtogroup BSP
 * @{
 */

#ifndef LAYERED_CAR_BSP_DWT_H
#define LAYERED_CAR_BSP_DWT_H

#include <stdint.h>

/**
 * @brief 使能 CoreDebug 跟踪与 DWT->CYCCNT 周期计数器。
 * @pre 须在系统时钟初始化（SystemCoreClock 就绪）后调用一次。
 * @warning 运行期禁止重复调用，否则会清零 DWT->CYCCNT 导致正在进行的时间差计算跳变。
 */
void BSP_DWT_Init(void);

/**
 * @brief 读取当前 DWT 32 位自由运行周期计数值。
 * @return 当前 CPU 时钟周期计数（单位：cycle，32 位无符号自然回绕）。
 * @pre 须已调用 BSP_DWT_Init()。
 * @note 可在任务与 ISR 上下文安全调用；计算时间差时请使用无符号减法 `(uint32_t)(now - start)`。
 */
uint32_t BSP_DWT_GetCycles(void);

/**
 * @brief 将 32 位无符号周期差换算为微秒（向零截断）。
 * @param[in] cycles 周期差值（单位：cycle，通常为 `(uint32_t)(end - start)`）。
 * @return 对应的微秒数（单位：us）；若 SystemCoreClock 为 0 则返回 0。
 * @note 内部采用 64 位乘法避免 `cycles * 1000000` 在高频下溢出。
 */
uint32_t BSP_DWT_CyclesToUs(uint32_t cycles);

/**
 * @brief 忙等指定微秒数。
 * @param[in] us 延时时长（单位：us）；传入 0 时立即返回。
 * @pre 须已调用 BSP_DWT_Init() 且 SystemCoreClock >= 1 MHz。
 * @note 内部按 1000 us 分片忙等以防止 32 位周期计数器乘法溢出。
 * @warning 仅用于总线时序或微秒级短延时，严禁在实时任务或 ISR 中执行长忙等。
 */
void BSP_DWT_DelayUs(uint32_t us);

/**
 * @brief 延时指定毫秒数。
 * @param[in] ms 延时时长（单位：ms）；传入 0 时立即返回。
 * @note 当启用 USE_FREERTOS 且内核处于 osKernelRunning 状态时调用 osDelay() 让出 CPU；
 *       在调度器启动前或裸机环境下退化为按 1000 us 分片的 DWT 忙等。
 * @warning 禁止在 ISR 中调用本函数（osDelay 不可在中断上下文执行）。
 */
void BSP_DelayMs(uint32_t ms);

#endif /* LAYERED_CAR_BSP_DWT_H */

/** @} */
