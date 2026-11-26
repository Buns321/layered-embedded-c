/**
 * @file bsp_pwm.h
 * @brief PWM 定时器通道描述符与防溢出占空比输出接口。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_PWM_H
#define LAYERED_CAR_BSP_PWM_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include <stdint.h>

/**
 * @brief PWM 通道描述符；max_duty 为 0 时按 ARR + 1 解释满占空比量程。
 */
typedef struct bsp_pwm_s {
  TIM_HandleTypeDef *htim;
  uint32_t channel;  /* TIM_CHANNEL_1..4 */
  uint32_t max_duty; /* 占空比量程（无量纲）；0 表示 ARR + 1 */
} bsp_pwm_t;

/**
 * @brief 启动指定 PWM 通道输出（仅任务上下文调用）。
 */
bsp_status_t BSP_PWM_Start(const bsp_pwm_t *pwm);

/**
 * @brief 停止指定 PWM 通道输出（仅任务上下文调用）。
 */
bsp_status_t BSP_PWM_Stop(const bsp_pwm_t *pwm);

/**
 * @brief 设置 PWM 占空比（duty 超出量程时夹紧，内部使用 64 位中间量防溢出）。
 */
bsp_status_t BSP_PWM_SetDuty(const bsp_pwm_t *pwm, uint32_t duty);

/** @} */

#endif /* LAYERED_CAR_BSP_PWM_H */
