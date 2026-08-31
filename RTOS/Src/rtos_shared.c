//
// Created by Buns_ on 2026/8/31.
//

#include "../Inc/rtos_shared.h"

PID_TypeDef       g_wheel_pid[4];
motor_tb6612_t    g_motor[4];
volatile uint32_t g_ctrl_heartbeat = 0;
