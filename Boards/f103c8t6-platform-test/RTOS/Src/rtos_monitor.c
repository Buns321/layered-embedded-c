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

/**
 * @cond F103_MONITOR_PLACEHOLDER_DUPLICATE_SYMBOL
 * @brief 监控任务入口（当前为最小占位循环）。
 * @param argument CMSIS-RTOS2 线程参数；实现未使用，可传 NULL。
 * @pre 由 RTOS 调度器以任务入口形式调用，且调度器已启动；不得在 ISR 中直接调用。
 * @post 函数不返回；每轮调用 osDelay(100)，约 100 ms 后再次运行。
 * @note 当前未接入遥测、状态日志、控制心跳或故障急停，也未在
 *       freertos.c 中创建对应任务；不能把本占位实现当作监控功能。
 * @warning 若在调度器启动前调用，osDelay 无法阻塞，循环会持续占用 CPU；
 *          实现不使用 assert，错误配置不会以断言终止呈现。
 * @endcond
 */
void RTOS_Monitor_StartTask(void *argument) {
  (void)argument;
  for (;;) {
    osDelay(100);
  }
}
