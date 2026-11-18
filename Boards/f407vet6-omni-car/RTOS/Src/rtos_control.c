/**
 * @file rtos_control.c
 * @brief CubeMX 静态控制任务的周期运行入口。
 * @ingroup rtos
 */

#include "rtos_tasks.h"
#include "rtos_periodic.h"
#include "rtos_config.h"
#include "app_car_runtime.h"

static const rtos_periodic_task_config_t s_control_loop = {
    .name = "ControlTask",
    .process = App_Car_ProcessControl,
    .period_ms = RTOS_CAR_CONTROL_PERIOD_MS,
};

/**
 * @brief 控制任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre CubeMX 已创建静态 ControlTask，App_Car_Init 已成功完成。
 * @post 永不返回，按配置周期调用 App_Car_ProcessControl。
 * @note 任务栈和 TCB 由 CubeMX 生成并静态分配。
 */
void RTOS_Control_StartTask(void *argument) {
  (void)argument;
  RTOS_Periodic_RunTask((void *)&s_control_loop);
}
