/**
 * @file bsp_pwm.c
 * @brief PWM 通道控制与占空比计算实现。
 * @ingroup bsp
 */

#include "bsp_pwm.h"
#include "bsp_hal_internal.h"
#include <stdbool.h>
#include <stddef.h>

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

  uint32_t arr = __HAL_TIM_GET_AUTORELOAD(pwm->htim);
  uint64_t effective_max = (pwm->max_duty > 0U) ? (uint64_t)pwm->max_duty : ((uint64_t)arr + 1ULL);
  uint64_t clamped_duty = ((uint64_t)duty > effective_max) ? effective_max : (uint64_t)duty;
  uint32_t ccr = (uint32_t)(((uint64_t)arr * clamped_duty) / effective_max);
  if (ccr > arr) {
    ccr = arr;
  }

  __HAL_TIM_SET_COMPARE(pwm->htim, pwm->channel, ccr);
  return BSP_OK;
}
