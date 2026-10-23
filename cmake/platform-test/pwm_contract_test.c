#include "platform_pwm_backend.h"
#include "platform_pwm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint32_t s_compare;

bool platform_pwm_backend_is_valid(Platform_PWM_Instance pwm) {
  return pwm == PLATFORM_PWM_TIM1;
}

Platform_StatusTypeDef platform_pwm_backend_start(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  (void)pwm;
  (void)channel;
  return Platform_OK;
}

Platform_StatusTypeDef platform_pwm_backend_start_all(
  Platform_PWM_Instance pwm
) {
  (void)pwm;
  return Platform_OK;
}

Platform_StatusTypeDef platform_pwm_backend_stop(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel
) {
  (void)pwm;
  (void)channel;
  return Platform_OK;
}

Platform_StatusTypeDef platform_pwm_backend_stop_all(
  Platform_PWM_Instance pwm
) {
  (void)pwm;
  return Platform_OK;
}

Platform_StatusTypeDef platform_pwm_backend_set_compare(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t compare
) {
  (void)pwm;
  (void)channel;
  s_compare = compare;
  return Platform_OK;
}

Platform_StatusTypeDef platform_pwm_backend_set_duty(
  Platform_PWM_Instance pwm,
  Platform_PWM_Channel channel,
  uint32_t max_duty,
  uint32_t duty
) {
  (void)pwm;
  (void)channel;
  s_compare = max_duty == 0U ? 0U : duty;
  return max_duty == 0U ? Platform_INVALID_ARGUMENT : Platform_OK;
}

static void test_pwm_contract(void) {
  s_compare = 0U;
  assert(Platform_PWM_Start(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1
         ) == Platform_OK);
  assert(Platform_PWM_SetCompare(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1,
           123U
         ) == Platform_OK);
  assert(s_compare == 123U);

  assert(Platform_PWM_SetCompare(
           PLATFORM_PWM_TIM1,
           (Platform_PWM_Channel)99U,
           1U
         ) == Platform_INVALID_ARGUMENT);
  assert((Platform_PWM_GetLastError(PLATFORM_PWM_TIM1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_2,
           1000U,
           500U
         ) == Platform_OK);
  assert(s_compare == 500U);
  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_2,
           0U,
           0U
         ) == Platform_INVALID_ARGUMENT);
  assert(Platform_PWM_Stop(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1
         ) == Platform_OK);
}

int main(void) {
  test_pwm_contract();
  return 0;
}
