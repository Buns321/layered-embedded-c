/**
 * @file test_car_omni.c
 * @brief car_omni 一键全向车门面端到端集成单元测试。
 * @ingroup tests
 */

#include "car_omni.h"
#include "rtos_task.h"
#include "sys_angle.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static TIM_TypeDef s_tim_enc_hw = {.is_32bit = false};
static TIM_TypeDef s_tim_pwm_hw = {.is_32bit = false};
static GPIO_TypeDef s_gpio_d;
static GPIO_TypeDef s_gpio_c;
static TIM_HandleTypeDef s_htim_pwm;
static TIM_HandleTypeDef s_htim_enc[4];

static car_omni_cfg_t make_base_cfg(void) {
  s_gpio_d = (GPIO_TypeDef){0};
  s_gpio_c = (GPIO_TypeDef){0};
  s_htim_pwm = (TIM_HandleTypeDef){.Instance = &s_tim_pwm_hw, .arr = 999U};
  const uint32_t ch[4] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3, TIM_CHANNEL_4};
  const uint16_t pins[8] = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3,
                            GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7};
  const float theta[4] = {SYS_PI * 1.75f, SYS_PI * 1.25f, SYS_PI * 0.75f, SYS_PI * 0.25f};

  car_omni_cfg_t cfg = {
      .control_period_s = 0.01f,
      .wheel_distance_m = 0.12f,
      .drive_frame = CHASSIS_FRAME_BODY,
  };
  for (uint8_t i = 0U; i < 4U; ++i) {
    s_htim_enc[i] = (TIM_HandleTypeDef){.Instance = &s_tim_enc_hw, .Init = {.Period = 0xFFFFU}};
    cfg.wheels[i] = (car_omni_wheel_cfg_t){
        .pwm = {.htim = &s_htim_pwm, .channel = ch[i], .max_duty = 1000U},
        .encoder = {.htim = &s_htim_enc[i]},
        .in1 = {&s_gpio_d, pins[(size_t)i * 2U]},
        .in2 = {&s_gpio_d, pins[((size_t)i * 2U) + 1U]},
        .stby = {&s_gpio_c, GPIO_PIN_13},
        .theta_rad = theta[i],
        .sign = 1.0f,
        .counts_per_meter = 1000.0f,
        .pid = {.kp = 1000.0f, .ki = 100.0f, .max_out = 1000.0f},
    };
  }
  return cfg;
}

static void push_wht(UART_HandleTypeDef *huart, uint8_t type, int16_t val) {
  uint8_t f[11] = {
      0x55U, type, 0U, 0U, 0U, 0U, (uint8_t)(val & 0xFF), (uint8_t)(((uint16_t)val >> 8U) & 0xFFU),
      0U,    0U,   0U};
  uint8_t sum = 0U;
  for (uint8_t i = 0U; i < 10U; ++i) {
    sum = (uint8_t)(sum + f[i]);
  }
  f[10] = sum;
  for (uint8_t i = 0U; i < 11U; ++i) {
    *huart->rx_buf = f[i];
    HAL_UART_RxCpltCallback(huart);
  }
}

static void push_rc(UART_HandleTypeDef *huart, const char *s) {
  for (size_t i = 0U; i < strlen(s); ++i) {
    *huart->rx_buf = (uint8_t)s[i];
    HAL_UART_RxCpltCallback(huart);
  }
}

static void test_body_mode_no_imu_no_rc(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();
  Sys_Daemon_ResetAll();

  car_omni_cfg_t cfg = make_base_cfg();
  car_omni_t car;
  assert(Car_Omni_Init(&car, &cfg) == BSP_OK);
  assert(Car_Omni_GetState(&car) == CAR_STATE_READY);
  assert(Car_Omni_Imu(&car) == NULL && Car_Omni_Chassis(&car) != NULL);

  const chassis_twist_t twist = {.vx_body_mps = 0.2f, .vy_body_mps = 0.0f, .wz_radps = 0.0f};
  Car_Omni_SetTwist(&car, &twist, 10U);
  Car_Omni_StepControl(&car, 10U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_RUNNING);
  assert(s_htim_pwm.ccr[0] > 0U);

  Car_Omni_Stop(&car);
  assert(Car_Omni_GetState(&car) == CAR_STATE_READY);
}

static void test_full_world_mode_and_safety_latches(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();
  Sys_Daemon_ResetAll();

  UART_HandleTypeDef huart_imu = {0};
  UART_HandleTypeDef huart_rc = {0};
  UART_HandleTypeDef huart_telem = {.gState = HAL_UART_STATE_READY};

  car_omni_cfg_t cfg = make_base_cfg();
  cfg.drive_frame = CHASSIS_FRAME_WORLD;
  cfg.imu_wht101 = (car_omni_imu_cfg_t){.huart = &huart_imu, .auto_zero_on_boot = true};
  cfg.rc_joycon =
      (car_omni_rc_cfg_t){.huart = &huart_rc, .timeout_ms = 300U, .deadband_norm = 0.03f};
  cfg.telemetry = (car_omni_telemetry_cfg_t){.huart = &huart_telem, .period_ms = 20U};

  car_omni_t car;
  assert(Car_Omni_Init(&car, &cfg) == BSP_OK);
  assert(Car_Omni_GetState(&car) == CAR_STATE_BOOTING);

  /* 推进 WHT101 自动归零状态机直至 CAR_STATE_READY。 */
  Car_Omni_ProcessInputs(&car, 0U);
  Car_Omni_ProcessInputs(&car, 200U);
  Car_Omni_ProcessInputs(&car, 300U);
  push_wht(&huart_imu, 0x52U, 5);
  push_wht(&huart_imu, 0x53U, 0);
  Car_Omni_ProcessInputs(&car, 350U);
  push_wht(&huart_imu, 0x52U, 5);
  push_wht(&huart_imu, 0x53U, 0);
  Car_Omni_ProcessInputs(&car, 760U);
  Car_Omni_ProcessInputs(&car, 770U);
  Car_Omni_ProcessInputs(&car, 970U);
  Car_Omni_ProcessInputs(&car, 1470U);
  push_wht(&huart_imu, 0x53U, 10);
  Car_Omni_ProcessInputs(&car, 1500U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_READY);

  /* 摇杆死区内保持停机。 */
  push_rc(&huart_rc, "[j,1,1,0,0]");
  Car_Omni_ProcessInputs(&car, 1510U);
  Car_Omni_DriveFromRC(&car, 1510U);
  Car_Omni_StepControl(&car, 1510U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_READY);

  /* 满偏摇杆验证速度圆限幅并输出遥测。 */
  push_rc(&huart_rc, "[j,100,100,0,0]");
  push_wht(&huart_imu, 0x53U, 0);
  Car_Omni_ProcessInputs(&car, 1520U);
  Car_Omni_DriveFromRC(&car, 1520U);
  Car_Omni_StepControl(&car, 1520U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_RUNNING);
  assert(hypotf(car.desired_twist.vx_body_mps, car.desired_twist.vy_body_mps) <=
         car.max_linear_mps + 1e-5f);
  Car_Omni_StepService(&car, 1520U);
  assert(huart_telem.tx_dma_calls == 1U);

  /* 遥控超时制动停机但不锁死 FAULT，可恢复；控制心跳超时永久锁存 FAULT。 */
  Car_Omni_DriveFromRC(&car, 1825U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_READY);
  Car_Omni_StepService(&car, 1825U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_FAULT);
  Car_Omni_SetTwist(&car, &(chassis_twist_t){.vx_body_mps = 0.1f}, 1830U);
  assert(Car_Omni_GetState(&car) == CAR_STATE_FAULT);
}

int main(void) {
  test_body_mode_no_imu_no_rc();
  test_full_world_mode_and_safety_latches();
  (void)puts("test_car_omni passed");
  return 0;
}
