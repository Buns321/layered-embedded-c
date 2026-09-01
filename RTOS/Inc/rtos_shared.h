//
// Created by Buns_ on 2026/8/31.
//

#ifndef LAYERED_EMBEDDED_C_RTOS_SHARED_H
#define LAYERED_EMBEDDED_C_RTOS_SHARED_H

#include "../../Algorithm/Inc/algorithm_pid_HongxiWong.h"
#include "../../DeviceDrivers/Inc/motor_tb6612.h"
#include "../../DeviceDrivers/Inc/encoder_tim.h"
#include "../../DeviceDrivers/Inc/led_gpio.h"

extern PID_TypeDef       g_wheel_pid[4];
extern motor_tb6612_t    g_motor[4];
extern volatile uint32_t g_ctrl_heartbeat;

#endif //LAYERED_EMBEDDED_C_RTOS_SHARED_H
