//
// Created by Buns_ on 2026/8/31.
//

#include "../Inc/rtos_shared.h"
#include "cmsis_os2.h"

PID_TypeDef        g_wheel_pid[4];
motor_tb6612_t     g_motor[4];
volatile uint32_t  g_ctrl_heartbeat = 0;
encoder_tim_t      g_encoder[4];
PID_TypeDef        g_yaw_pid;
imu_wht101_usart_t g_wht101;
usart_controller_t g_controller;

void App_Init(void) {
  // 电机初始化 ****************************************************************
  Dev_Motor_TB6612_Init(
    &g_motor[0], "m0", 1000,
    PLATFORM_TIM_TIM1, PLATFORM_TIM_CHANNEL_1,
    PLATFORM_GPIO_PORT_D, 1u<<0,
    PLATFORM_GPIO_PORT_D, 1u<<1,
    PLATFORM_GPIO_PORT_D, 1u<<8);
  Dev_Motor_TB6612_Init(
    &g_motor[1], "m1", 1000,
    PLATFORM_TIM_TIM1, PLATFORM_TIM_CHANNEL_2,
    PLATFORM_GPIO_PORT_D, 1u<<2,
    PLATFORM_GPIO_PORT_D, 1u<<3,
    PLATFORM_GPIO_PORT_D, 1u<<8);
  Dev_Motor_TB6612_Init(
    &g_motor[2], "m2", 1000,
    PLATFORM_TIM_TIM1, PLATFORM_TIM_CHANNEL_3,
    PLATFORM_GPIO_PORT_D, 1u<<4,
    PLATFORM_GPIO_PORT_D, 1u<<5,
    PLATFORM_GPIO_PORT_D, 1u<<9);
  Dev_Motor_TB6612_Init(
    &g_motor[3], "m3", 1000,
    PLATFORM_TIM_TIM1, PLATFORM_TIM_CHANNEL_4,
    PLATFORM_GPIO_PORT_D, 1u<<6,
    PLATFORM_GPIO_PORT_D, 1u<<7,
    PLATFORM_GPIO_PORT_D, 1u<<9);

  // 编码器初始化 **************************************************************
  Dev_Encoder_TIM_Init(
    &g_encoder[0], "e0",
    PLATFORM_TIM_TIM2, PLATFORM_TIM_CHANNEL_ALL);
  Dev_Encoder_TIM_Init(
    &g_encoder[1], "e1",
    PLATFORM_TIM_TIM3, PLATFORM_TIM_CHANNEL_ALL);
  Dev_Encoder_TIM_Init(
    &g_encoder[2], "e2",
    PLATFORM_TIM_TIM4, PLATFORM_TIM_CHANNEL_ALL);
  Dev_Encoder_TIM_Init(
    &g_encoder[3], "e3",
    PLATFORM_TIM_TIM5, PLATFORM_TIM_CHANNEL_ALL);

  // 电机PID初始化 *************************************************************
  for (int i = 0; i < 4; i++) {
    PID_Init(
      &g_wheel_pid[i],
      1000,   /* max_out：PWM 范围 ±1000 */
      500,    /* integral_limit：先给 500，后面调 */
      0.0f,   /* deadband：先 0 */
      1.0f, 0.0f, 0.0f,   /* Kp Ki Kd：先给占位，后面调参 */
      0.0f, 0.0f,         /* 变积分 A B：先不用 */
      1.0f, 1.0f,         /* 输出/微分滤波系数：1 = 不滤波 */
      ErrorHandle | Integral_Limit);  /* 开堵转检测 + 积分限幅 */
  }

  // 航向角PID初始化 ***********************************************************
  PID_Init(
    &g_yaw_pid,
    200,    /* max_out：ω 修正范围，先给 ±200 */
    100,    /* integral_limit */
    0.0f,
    1.0f, 0.0f, 0.0f,   /* Kp Ki Kd 占位，后面调 */
    0.0f, 0.0f,
    1.0f, 1.0f,
    Integral_Limit);

  // WHT101 IMU初始化 **********************************************************
  Dev_IMU_WHT101_USART_Init(&g_wht101, "wht101", PLATFORM_USART_1);

  // 串口控制器初始化 **********************************************************
  Dev_USART_Controller_Init(&g_controller, PLATFORM_USART_2);
}
