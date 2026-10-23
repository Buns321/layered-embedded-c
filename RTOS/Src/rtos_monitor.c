//
// 由 Buns_ 创建于 2026/8/31。
//

#include "rtos_monitor.h"

#include "cmsis_os2.h"

void StartMonitor_Task(void *argument) {
  (void)argument;
  for (;;) {
    osDelay(100);
  }
}
