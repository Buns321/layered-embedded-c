/**
 * @file test_motor_tb6612.c
 * @brief motor_base / motor_tb6612 / led 宿主机单元测试。
 * @ingroup tests
 */

#include "controller_pid.h"
#include "led.h"
#include "motor_tb6612.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static TIM_TypeDef s_tim_enc_hw = {.is_32bit = false};
static TIM_TypeDef s_tim_pwm_hw = {.is_32bit = false};
static TIM_HandleTypeDef s_htim_pwm;
static uint32_t s_dir_change_hook_calls;

static void verify_pwm_zero_before_gpio(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state) {
  (void)port;
  (void)state;
  if (pin == GPIO_PIN_0 || pin == GPIO_PIN_1) {
    assert(s_htim_pwm.ccr[0] == 0U);
    s_dir_change_hook_calls++;
  }
}

static void test_tb6612_direction_brake_and_closed_loop(void) {
  BSP_Stub_ResetAll();
  s_dir_change_hook_calls = 0U;

  GPIO_TypeDef port_d = {0};
  GPIO_TypeDef port_c = {0};
  TIM_HandleTypeDef htim_enc = {.Instance = &s_tim_enc_hw, .Init = {.Period = 0xFFFFU}};
  s_htim_pwm = (TIM_HandleTypeDef){.Instance = &s_tim_pwm_hw, .arr = 999U};

  bsp_encoder_t enc;
  assert(BSP_Encoder_Init(&enc, &(bsp_encoder_cfg_t){.htim = &htim_enc}) == BSP_OK);

  controller_pid_t pid;
  assert(Ctrl_PID_Init(&pid, "pid", &(ctrl_pid_cfg_t){.kp = 1000.0f, .max_out = 1000.0f}));

  motor_tb6612_t motor;
  const motor_tb6612_cfg_t cfg = {
      .name = "m0",
      .pwm = {.htim = &s_htim_pwm, .channel = TIM_CHANNEL_1, .max_duty = 0U},
      .in1 = {&port_d, GPIO_PIN_0},
      .in2 = {&port_d, GPIO_PIN_1},
      .stby = {&port_c, GPIO_PIN_13},
      .encoder = &enc,
      .controller = &pid.base,
      .counts_per_meter = 1000.0f,
      .control_period_s = 0.01f,
  };
  assert(Dev_Motor_TB6612_Init(&motor, &cfg) == BSP_OK);
  assert(motor.base.max_duty == 1000U);
  assert(HAL_GPIO_ReadPin(&port_c, GPIO_PIN_13) == GPIO_PIN_RESET);

  BSP_Stub_SetGpioWriteHook(verify_pwm_zero_before_gpio);

  /* 使能并正转更新。 */
  Dev_Motor_Base_Enable(&motor.base);
  assert(HAL_GPIO_ReadPin(&port_c, GPIO_PIN_13) == GPIO_PIN_SET);
  Dev_Motor_Base_SetTargetSpeed(&motor.base, 0.5f);
  htim_enc.cnt = 0U;
  Dev_Motor_Base_Update(&motor.base);
  assert(HAL_GPIO_ReadPin(&port_d, GPIO_PIN_0) == GPIO_PIN_SET);
  assert(HAL_GPIO_ReadPin(&port_d, GPIO_PIN_1) == GPIO_PIN_RESET);
  assert(s_htim_pwm.ccr[0] > 0U);

  /* 反转前强制先撤 PWM=0。 */
  Dev_Motor_Base_SetTargetSpeed(&motor.base, -0.5f);
  htim_enc.cnt = 10U;
  Dev_Motor_Base_Update(&motor.base);
  assert(fabsf(Dev_Motor_Base_ReadCurrentSpeed(&motor.base) - 1.0f) < 1e-4f);
  assert(HAL_GPIO_ReadPin(&port_d, GPIO_PIN_0) == GPIO_PIN_RESET);
  assert(HAL_GPIO_ReadPin(&port_d, GPIO_PIN_1) == GPIO_PIN_SET);
  assert(s_htim_pwm.ccr[0] == 999U);

  /* 短路制动：先写 PWM=0，再写 IN1=IN2=HIGH，最后写满占空比。 */
  Dev_Motor_Base_Brake(&motor.base);
  assert(HAL_GPIO_ReadPin(&port_d, GPIO_PIN_0) == GPIO_PIN_SET);
  assert(HAL_GPIO_ReadPin(&port_d, GPIO_PIN_1) == GPIO_PIN_SET);
  assert(s_htim_pwm.ccr[0] == 999U);
  assert(s_dir_change_hook_calls >= 6U);

  /* 紧急断电钩子清零输出与 STBY。 */
  Dev_Motor_TB6612_EmergencyHalt(&motor);
  assert(s_htim_pwm.ccr[0] == 0U);
  assert(HAL_GPIO_ReadPin(&port_c, GPIO_PIN_13) == GPIO_PIN_RESET);
  assert(Dev_Motor_Base_ReadStatus(&motor.base) == DEV_MOTOR_DISABLE);
}

static void test_optional_stby_and_led(void) {
  BSP_Stub_ResetAll();
  GPIO_TypeDef port = {0};
  dev_led_t led_gpio;
  assert(Dev_LED_Init(&led_gpio, &(dev_led_cfg_t){
                                     .mode = DEV_LED_MODE_GPIO,
                                     .gpio = {&port, GPIO_PIN_5},
                                     .active_high = false,
                                 }) == BSP_OK);
  assert(HAL_GPIO_ReadPin(&port, GPIO_PIN_5) == GPIO_PIN_SET);
  Dev_LED_Set(&led_gpio, true);
  assert(Dev_LED_IsOn(&led_gpio) && HAL_GPIO_ReadPin(&port, GPIO_PIN_5) == GPIO_PIN_RESET);
  Dev_LED_Toggle(&led_gpio);
  assert(!Dev_LED_IsOn(&led_gpio) && HAL_GPIO_ReadPin(&port, GPIO_PIN_5) == GPIO_PIN_SET);

  TIM_HandleTypeDef htim_led = {.Instance = &s_tim_pwm_hw, .arr = 999U};
  dev_led_t led_pwm;
  assert(Dev_LED_Init(&led_pwm, &(dev_led_cfg_t){
                                    .mode = DEV_LED_MODE_PWM,
                                    .pwm = {.htim = &htim_led, .channel = TIM_CHANNEL_2},
                                }) == BSP_OK);
  Dev_LED_SetBrightness(&led_pwm, 32768U);
  assert(Dev_LED_IsOn(&led_pwm) && htim_led.ccr[1] >= 498U && htim_led.ccr[1] <= 501U);
}

int main(void) {
  test_tb6612_direction_brake_and_closed_loop();
  test_optional_stby_and_led();
  (void)puts("test_motor_tb6612 passed");
  return 0;
}
