//
// Created by Buns_ on 2026/8/31.
//

#ifndef LAYERED_EMBEDDED_C_RTOS_SHARED_H
#define LAYERED_EMBEDDED_C_RTOS_SHARED_H

#include "../../Algorithm/Inc/algorithm_pid_HongxiWong.h"
#include "../../DeviceDrivers/Inc/motor_tb6612.h"
#include "../../DeviceDrivers/Inc/encoder_tim.h"
#include "../../DeviceDrivers/Inc/usart_controller.h"
#include "../../DeviceDrivers/Inc/imu_wht101_usart.h"

extern PID_TypeDef        g_wheel_pid[4];
extern motor_tb6612_t     g_motor[4];
extern volatile uint32_t  g_ctrl_heartbeat;
extern encoder_tim_t      g_encoder[4];

extern imu_wht101_usart_t g_wht101;
extern usart_controller_t g_controller;

void App_Init(void);

#endif //LAYERED_EMBEDDED_C_RTOS_SHARED_H
