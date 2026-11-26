/**
 * @file rtos_tasks.c
 * @brief F407 静态周期任务声明与集中启动实现。
 * @details 基于 Framework/Core 的 RTOS_TASK_PERIODIC 宏声明三路固定周期任务，
 *          仅作薄调度声明与周期转发，不包含任何业务逻辑或外设直接访问。
 */

#include "rtos_tasks.h"
#include "app_car.h"
#include "rtos_task.h"

/** @brief 控制闭环任务：10 ms（100 Hz），最高实时优先级，栈 1024 words（4096 B）。 */
RTOS_TASK_PERIODIC(s_control_task, 10U, osPriorityRealtime, 1024U, App_Car_StepControl);

/** @brief 传感器与遥控输入任务：2 ms（500 Hz），普通优先级，栈 768 words（3072 B）。 */
RTOS_TASK_PERIODIC(s_input_task, 2U, osPriorityNormal, 768U, App_Car_StepInput);

/** @brief 后台服务任务：10 ms（100 Hz），低优先级，栈 512 words（2048 B）。 */
RTOS_TASK_PERIODIC(s_service_task, 10U, osPriorityLow, 512U, App_Car_StepService);

bool RTOS_StartAllTasks(void) {
  // 按控制、输入、服务顺序拉起静态周期任务；任一失败即短路返回 false
  // 按控制、输入、服务顺序拉起静态周期任务；任一失败即短路返回 false
  return RTOS_Task_StartPeriodic(&s_control_task) && RTOS_Task_StartPeriodic(&s_input_task) &&
         RTOS_Task_StartPeriodic(&s_service_task);
}
