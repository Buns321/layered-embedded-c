/**
 * @file pwm_contract_test.c
 * @brief PWM Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_pwm_backend.h"
#include "platform_pwm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint32_t s_compare;
static platform_pwm_instance_t s_last_pwm;

bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm) {
  return pwm == PLATFORM_PWM_TIM1 ||
         pwm == PLATFORM_PWM_TIM2 ||
         pwm == PLATFORM_PWM_TIM3 ||
         pwm == PLATFORM_PWM_TIM4;
}

platform_status_t platform_pwm_backend_start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  (void)channel;
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_pwm_backend_start_all(
  platform_pwm_instance_t pwm
) {
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_pwm_backend_stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  (void)channel;
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_pwm_backend_stop_all(
  platform_pwm_instance_t pwm
) {
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_pwm_backend_set_compare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
) {
  (void)channel;
  s_last_pwm = pwm;
  s_compare = compare;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_pwm_backend_set_duty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
) {
  (void)channel;
  s_last_pwm = pwm;
  s_compare = max_duty == 0U ? 0U : duty;
  return max_duty == 0U ? PLATFORM_STATUS_INVALID_ARGUMENT : PLATFORM_STATUS_OK;
}

static void test_pwm_contract(void) {
  s_compare = 0U;
  s_last_pwm = (platform_pwm_instance_t)0xFFU;
  assert(Platform_PWM_Start(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
  assert(Platform_PWM_SetCompare(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1,
           123U
         ) == PLATFORM_STATUS_OK);
  assert(s_compare == 123U);

  assert(Platform_PWM_Start(
           PLATFORM_PWM_TIM2,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
  assert(s_last_pwm == PLATFORM_PWM_TIM2);
  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM4,
           PLATFORM_PWM_CHANNEL_3,
           20000U,
           1500U
         ) == PLATFORM_STATUS_OK);
  assert(s_last_pwm == PLATFORM_PWM_TIM4);
  assert(s_compare == 1500U);
  assert(Platform_PWM_StopAll(PLATFORM_PWM_TIM3) == PLATFORM_STATUS_OK);
  assert(s_last_pwm == PLATFORM_PWM_TIM3);

  assert(Platform_PWM_SetCompare(
           PLATFORM_PWM_TIM1,
           (platform_pwm_channel_t)99U,
           1U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_PWM_GetLastError(PLATFORM_PWM_TIM1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_2,
           1000U,
           500U
         ) == PLATFORM_STATUS_OK);
  assert(s_compare == 500U);
  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_2,
           0U,
           0U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_PWM_Stop(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
  assert(Platform_PWM_Stop(
           PLATFORM_PWM_TIM2,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
}

int main(void) {
  test_pwm_contract();
  return 0;
}
