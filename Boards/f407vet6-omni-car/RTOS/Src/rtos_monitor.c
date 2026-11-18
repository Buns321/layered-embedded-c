/**
 * @file rtos_monitor.c
 * @brief CubeMX 静态监控任务的周期运行入口。
 * @ingroup rtos
 */

#include "rtos_tasks.h"
#include "rtos_periodic.h"
#include "rtos_config.h"
#include "app_car_runtime.h"

static const rtos_periodic_task_config_t s_monitor_loop = {
    .name = "MonitorTask",
    .process = App_Car_ProcessMonitor,
    .period_ms = RTOS_CAR_MONITOR_PERIOD_MS,
};

/**
 * @brief 监控任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre CubeMX 已创建静态 MonitorTask，App_Car_Init 已成功完成。
 * @post 永不返回，按配置周期调用 App_Car_ProcessMonitor。
 * @note 任务栈和 TCB 由 CubeMX 生成并静态分配。
 */
void RTOS_Monitor_StartTask(void *argument) {
  (void)argument;
  RTOS_Periodic_RunTask((void *)&s_monitor_loop);
}
