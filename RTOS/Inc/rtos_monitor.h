//
// Created by Buns_ on 2026/8/31.
//

#ifndef LAYERED_EMBEDDED_C_RTOS_MONITOR_H
#define LAYERED_EMBEDDED_C_RTOS_MONITOR_H
#include "../Inc/rtos_shared.h"

typedef enum {
  MONITOR_NORMAL = 0,
  MONITOR_MOTOR_BLOCKED,
  MONITOR_CTRL_DEAD,
} monitor_state_t;

void StartMonitor_Task(void *argument);

#endif //LAYERED_EMBEDDED_C_RTOS_MONITOR_H
