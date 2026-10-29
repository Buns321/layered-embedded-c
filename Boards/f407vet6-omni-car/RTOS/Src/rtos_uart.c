//
// 由 Buns_ 创建于 2026/9/2。
//

/**
 * @file rtos_uart.c
 * @brief RTOS UART 任务实现。
 * @ingroup rtos
 */

#include "rtos_uart.h"
#include "app_car.h"
#include "cmsis_os2.h"
#include "rtos_config.h"
#include "system_log.h"

/**
 * @brief WHT101 与 JoyCon 的 UART 处理任务。
 * @param argument CMSIS-RTOS2 任务参数，当前未使用。
 * @note 中断只负责入队；协议解析和上电归零状态机在本任务上下文运行。
 */
void RTOS_UART_StartTask(void *argument) {
  (void)argument;

  SYS_LOG_INFO("RTOS", "uart task started");

  uint32_t next_wake_tick = osKernelGetTickCount();
  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    if ((uint32_t)(now_tick - next_wake_tick) > RTOS_CAR_UART_PERIOD_TICKS) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += RTOS_CAR_UART_PERIOD_TICKS;

    App_Car_ProcessInputs(now_tick);
    (void)osDelayUntil(next_wake_tick);
  }
}
