/**
 * @file rtos_tasks.c
 * @brief F407 静态周期任务声明与集中启动实现。
 */

#include "rtos_tasks.h"
#include "app_car.h"
#include "rtos_task.h"

RTOS_TASK_PERIODIC(s_control_task, 10U, osPriorityRealtime, 1024U, App_Car_StepControl);
RTOS_TASK_PERIODIC(s_input_task, 2U, osPriorityNormal, 768U, App_Car_StepInput);
RTOS_TASK_PERIODIC(s_service_task, 10U, osPriorityLow, 512U, App_Car_StepService);

bool RTOS_StartAllTasks(void) {
  return RTOS_Task_StartPeriodic(&s_control_task) && RTOS_Task_StartPeriodic(&s_input_task) &&
         RTOS_Task_StartPeriodic(&s_service_task);
}
