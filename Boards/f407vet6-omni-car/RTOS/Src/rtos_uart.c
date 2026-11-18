/**
 * @file rtos_uart.c
 * @brief CubeMX 静态输入任务的周期运行入口。
 * @ingroup rtos
 */

#include "rtos_tasks.h"
#include "rtos_periodic.h"
#include "rtos_config.h"
#include "app_car_runtime.h"

static const rtos_periodic_task_config_t s_input_loop = {
    .name = "UARTTask",
    .process = App_Car_ProcessInputs,
    .period_ms = RTOS_CAR_INPUT_PERIOD_MS,
};

/**
 * @brief 输入任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre CubeMX 已创建静态 UARTTask，App_Car_Init 已成功完成。
 * @post 永不返回，按配置周期调用 App_Car_ProcessInputs。
 * @note 任务栈和 TCB 由 CubeMX 生成并静态分配。
 */
void RTOS_UART_StartTask(void *argument) {
  (void)argument;
  RTOS_Periodic_RunTask((void *)&s_input_loop);
}
