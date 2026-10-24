//
// 由 Buns_ 创建于 2026/8/31。
//

/**
 * @file rtos_monitor.c
 * @brief RTOS 监控任务实现。
 * @ingroup rtos
 */

#include "rtos_monitor.h"

#include "cmsis_os2.h"

void RTOS_Monitor_StartTask(void *argument) {
  (void)argument;
  for (;;) {
    osDelay(100);
  }
}
