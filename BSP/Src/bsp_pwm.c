/**
 * @file bsp_pwm.c
 * @brief PWM 通道控制与占空比计算实现。
 * @details 校验 PWM 通道描述符并通过 64 位整数乘法将逻辑占空比换算为硬件 CCR 值，
 *          避免 32 位中间溢出与比较值越界。
 * @ingroup BSP
 */

#include "bsp_pwm.h"
#include "bsp_hal_internal.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 校验 PWM 通道描述符是否合法。
 * @param[in] pwm PWM 通道描述符指针。
 * @return 是否有效。
 * @retval true  `pwm` 与 `htim` 非空且通道为 `TIM_CHANNEL_1..4` 之一。
 * @retval false 参数为空或通道号非法。
 */
static bool bsp_pwm_isvalid(const bsp_pwm_t *pwm) {
  if (pwm == NULL || pwm->htim == NULL) {
    return false;
  }
  return pwm->channel == TIM_CHANNEL_1 || pwm->channel == TIM_CHANNEL_2 ||
         pwm->channel == TIM_CHANNEL_3 || pwm->channel == TIM_CHANNEL_4;
}

bsp_status_t BSP_PWM_Start(const bsp_pwm_t *pwm) {
  if (!bsp_pwm_isvalid(pwm)) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_status_from_hal(HAL_TIM_PWM_Start(pwm->htim, pwm->channel));
}

bsp_status_t BSP_PWM_Stop(const bsp_pwm_t *pwm) {
  if (!bsp_pwm_isvalid(pwm)) {
    return BSP_ERR_INVALID_ARG;
  }
  return bsp_status_from_hal(HAL_TIM_PWM_Stop(pwm->htim, pwm->channel));
}

bsp_status_t BSP_PWM_SetDuty(const bsp_pwm_t *pwm, uint32_t duty) {
  if (!bsp_pwm_isvalid(pwm)) {
    return BSP_ERR_INVALID_ARG;
  }

  // 读取硬件自动重装载值 ARR；当未配置逻辑 max_duty 时以 ARR + 1 作为满量程
  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pwm->htim);
  uint64_t effective_max = (pwm->max_duty > 0U) ? (uint64_t)pwm->max_duty : ((uint64_t)arr + 1ULL);
  // 先对逻辑占空比夹紧，再使用 64 位乘法防 arr * clamped_duty 溢出 32 位
  uint64_t clamped_duty = ((uint64_t)duty > effective_max) ? effective_max : (uint64_t)duty;
  uint32_t ccr = (uint32_t)(((uint64_t)arr * clamped_duty) / effective_max);
  // 兜底限制 ccr 不超过 ARR，确保比较输出行为与定时器周期一致
  if (ccr > arr) {
    ccr = arr;
  }

  __HAL_TIM_SET_COMPARE(pwm->htim, pwm->channel, ccr);
  return BSP_OK;
}
