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
#include "app_car_config.h"
#include "cmsis_os2.h"
#include "system_log.h"

void RTOS_UART_StartTask(void *argument) {
  (void)argument;

  SYS_LOG_INFO("RTOS", "uart task started");

  uint32_t next_wake_tick = osKernelGetTickCount();
  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    if ((uint32_t)(now_tick - next_wake_tick) > APP_CAR_UART_PERIOD_TICKS) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += APP_CAR_UART_PERIOD_TICKS;

    App_Car_ProcessInputs(now_tick);
    (void)osDelayUntil(next_wake_tick);
  }
}
