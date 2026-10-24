/**
 * @file board_selftest.h
 * @brief F103C8T6 非破坏性板载自检入口。
 * @addtogroup board
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_BOARD_SELFTEST_H
#define LAYERED_EMBEDDED_C_BOARD_SELFTEST_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 运行一次非破坏性板载自检。
 * @param scheduler_tick_ok 调用方测量得到的调度器 tick 健康状态。
 * @param scheduler_tick_elapsed_ms 调用方测量得到的调度器 tick 耗时（ms）。
 * @return 全部测试通过时返回 true。
 * @note 必须在 RTOS 调度器启动后、任务上下文中调用。
 * @note 测试结果通过 System 日志输出，不操作继电器、电机、舵机、蜂鸣器或灯。
 */
bool Board_SelfTest_RunAll(
  bool scheduler_tick_ok,
  uint32_t scheduler_tick_elapsed_ms
);

/** @} */

#endif //LAYERED_EMBEDDED_C_BOARD_SELFTEST_H
