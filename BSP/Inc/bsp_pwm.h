/**
 * @file bsp_pwm.h
 * @brief PWM 定时器通道描述符与防溢出占空比输出接口。
 * @details 封装 STM32 HAL 定时器句柄、通道编号与逻辑满占空比量程，
 *          内部通过 64 位整数乘法换算比较寄存器 CCR 值以防止 32 位中间溢出。
 *          本模块无动态内存分配，与上层执行器解耦。
 *
 * @addtogroup BSP
 * @{
 */

#ifndef LAYERED_CAR_BSP_PWM_H
#define LAYERED_CAR_BSP_PWM_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include <stdint.h>

/**
 * @brief PWM 通道描述符。
 * @details `max_duty` 为 0 时按定时器重装载值 `ARR + 1` 解释满占空比量程。
 */
typedef struct bsp_pwm_s {
  TIM_HandleTypeDef *htim; /**< 定时器 HAL 句柄指针（非 NULL，须已完成 CubeMX PWM 初始化）。 */
  uint32_t channel;        /**< 定时器通道编号（仅支持 TIM_CHANNEL_1..TIM_CHANNEL_4）。 */
  uint32_t max_duty;       /**< 逻辑占空比满量程（无量纲，对应 `_duty`）；0 表示采用 ARR + 1。 */
} bsp_pwm_t;

/**
 * @brief 启动指定 PWM 通道输出。
 * @param[in] pwm PWM 通道描述符指针（不允许为 NULL）。
 * @return 操作状态码。
 * @retval BSP_OK              通道启动成功。
 * @retval BSP_ERR_INVALID_ARG `pwm`、`htim` 为 NULL 或通道号非法。
 * @retval BSP_ERR_HAL         底层 HAL_TIM_PWM_Start() 返回失败。
 * @pre 须在 CubeMX 定时器初始化完成后调用。
 * @note 仅限任务上下文调用，禁止在 ISR 中启停定时器通道。
 */
bsp_status_t BSP_PWM_Start(const bsp_pwm_t *pwm);

/**
 * @brief 停止指定 PWM 通道输出。
 * @param[in] pwm PWM 通道描述符指针（不允许为 NULL）。
 * @return 操作状态码。
 * @retval BSP_OK              通道停止成功。
 * @retval BSP_ERR_INVALID_ARG `pwm`、`htim` 为 NULL 或通道号非法。
 * @retval BSP_ERR_HAL         底层 HAL_TIM_PWM_Stop() 返回失败。
 * @note 仅限任务上下文调用。
 */
bsp_status_t BSP_PWM_Stop(const bsp_pwm_t *pwm);

/**
 * @brief 设置 PWM 通道占空比。
 * @param[in] pwm  PWM 通道描述符指针（不允许为 NULL）。
 * @param[in] duty 目标占空比（无量纲，范围 `[0, effective_max]`；超出部分自动夹紧）。
 * @return 操作状态码。
 * @retval BSP_OK              CCR 比较寄存器更新成功。
 * @retval BSP_ERR_INVALID_ARG `pwm`、`htim` 为 NULL 或通道号非法。
 * @note 内部以 64 位整数计算 `ccr = (arr * clamped_duty) / effective_max` 防止溢出，
 *       并确保 `ccr <= arr`；可在任务或紧急断电回调中调用。
 */
bsp_status_t BSP_PWM_SetDuty(const bsp_pwm_t *pwm, uint32_t duty);

#endif /* LAYERED_CAR_BSP_PWM_H */

/** @} */
