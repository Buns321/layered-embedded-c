/**
 * @file board_cfg.c
 * @brief F407 四轮全向小车硬件映射、服务启动与静态装配。
 */

#include "board_cfg.h"
#include "bsp_dwt.h"
#include "gpio.h"
#include "sys_angle.h"
#include "sys_log.h"
#if defined(SYS_LOG_USE_RTT)
#include "sys_log_rtt.h"
#endif
#include "tim.h"
#include "usart.h"
#include <stddef.h>

#define BOARD_WHEEL_PID_CFG                                                                        \
  {                                                                                                \
      .kp = 1000.0f,                                                                               \
      .ki = 2000.0f,                                                                               \
      .kd = 5.0f,                                                                                  \
      .max_target = 1.0f,                                                                          \
      .max_out = 1000.0f,                                                                          \
      .max_integral = 400.0f,                                                                      \
      .derivative_tau_s = 0.02f,                                                                   \
  }

static car_omni_t s_car;
#if defined(SYS_LOG_USE_RTT)
static sys_log_rtt_t s_log_rtt;
#endif

static const car_omni_cfg_t s_car_cfg = {
    .control_period_s = 0.01f,
    .wheel_distance_m = 0.12f,
    .max_linear_mps = 0.30f,
    .max_angular_radps = 1.50f,
    .drive_frame = CHASSIS_FRAME_WORLD,
    .wheels =
        {
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_1, .max_duty = 1000U},
                .encoder = {.htim = &htim2},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_0},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_1},
                .stby = {.port = GPIOC, .pin = GPIO_PIN_13},
                .theta_rad = SYS_PI * 1.75f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_2, .max_duty = 1000U},
                .encoder = {.htim = &htim3},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_2},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_3},
                .stby = {.port = GPIOC, .pin = GPIO_PIN_13},
                .theta_rad = SYS_PI * 1.25f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_3, .max_duty = 1000U},
                .encoder = {.htim = &htim4},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_4},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_5},
                .stby = {.port = GPIOE, .pin = GPIO_PIN_5},
                .theta_rad = SYS_PI * 0.75f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_4, .max_duty = 1000U},
                .encoder = {.htim = &htim5},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_6},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_7},
                .stby = {.port = GPIOE, .pin = GPIO_PIN_5},
                .theta_rad = SYS_PI * 0.25f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
        },
    .imu_wht101 = {.huart = &huart1, .timeout_ms = 250U, .auto_zero_on_boot = true},
    .rc_joycon = {.huart = &huart3, .timeout_ms = 300U, .deadband_norm = 0.03f},
    .telemetry = {.huart = &huart2, .period_ms = 20U, .use_firewater = false},
    .control_heartbeat_timeout_ms = 100U,
};

bool Board_BootServices(void) {
  BSP_DWT_Init();
#if defined(SYS_LOG_USE_RTT)
  Sys_Log_RTT_Init(&s_log_rtt, "rtt", 0U);
  Sys_Log_Init(&s_log_rtt.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
  SYS_LOG_INFO("BOOT", "RTT log initialized");
#endif
  return true;
}

const car_omni_cfg_t *Board_GetCarConfig(void) { return &s_car_cfg; }

car_omni_t *Board_InitCar(void) {
  bsp_status_t st = Car_Omni_Init(&s_car, &s_car_cfg);
  if (st != BSP_OK) {
    SYS_LOG_ERROR("BOARD", "car init failed: %u", (unsigned int)st);
    return NULL;
  }
  return &s_car;
}
