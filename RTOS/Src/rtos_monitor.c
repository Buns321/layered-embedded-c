//
// Created by Buns_ on 2026/8/31.
//

#include "../Inc/rtos_monitor.h"

#include "cmsis_os2.h"

static volatile monitor_state_t g_monitor_state;

void StartMonitor_Task(void *argument) {
  uint32_t last_wake = osKernelGetTickCount();
  volatile uint32_t last_heartbeat = 0;
  for (;;) {
    last_wake += 100;
    osDelayUntil(last_wake);
    // g_ctrl_heartbeat ++;

    // 检测控制任务是否卡死，若卡死，则停机
    if (last_heartbeat == g_ctrl_heartbeat) {
      // stop all
      g_monitor_state = MONITOR_CTRL_DEAD;
    }
    last_heartbeat = g_ctrl_heartbeat;

    // 检测电机是否抛出阻塞，若阻塞，则停机
    for (uint8_t i = 0; i < 4; i++) {
      if (g_wheel_pid[i].ERRORHandler.ERRORType == Motor_Blocked) {
        g_monitor_state = MONITOR_MOTOR_BLOCKED;
        // stop i
        g_wheel_pid[i].ERRORHandler.ERRORType = PID_ERROR_NONE;
        g_wheel_pid[i].ERRORHandler.ERRORCount = 0;
      }
    }
    // led(g_monitor_state)
  }
}
