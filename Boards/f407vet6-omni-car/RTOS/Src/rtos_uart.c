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
 * @brief WHT101 与 JoyCon 的 500 Hz UART 处理任务入口。
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅在调度器启动后由内核调用，且 App_Car_Init 已完成 UART、IMU 和 JoyCon 初始化。
 * @post 每 2 tick 调用一次 App_Car_ProcessInputs，消费 ISR 入队字节并推进速率配置、静止确认和归零状态机。
 * @note 中断只负责接收入队；协议解析和 IMU 设备请求在本任务上下文运行，不分配内存。
 * @note 错过多个周期时只从当前 tick 重新排程；正常运行时永不返回，不阻塞等待 UART 数据。
 * @warning 本任务停摆会导致 IMU/JoyCon 数据老化；控制任务会因数据失效而制动，但急停仍依赖监控任务。
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
