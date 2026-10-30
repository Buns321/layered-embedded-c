/**
 * @file FreeRTOS.h
 * @brief System 临界区契约测试使用的最小 FreeRTOS 配置桩。
 * @ingroup tests
 */

#ifndef LAYERED_EMBEDDED_C_TEST_FREERTOS_H
#define LAYERED_EMBEDDED_C_TEST_FREERTOS_H

#include <assert.h>
#include <stdint.h>

#define configASSERT(value) assert(value)
#define taskSCHEDULER_NOT_STARTED 0
#define taskSCHEDULER_RUNNING 1

typedef uint32_t UBaseType_t;

extern uint32_t g_test_primask;
extern uint32_t g_test_scheduler_state;
extern uint32_t g_test_task_enter_count;
extern uint32_t g_test_task_exit_count;
extern uint32_t g_test_isr_enter_count;
extern uint32_t g_test_isr_exit_count;

/**
 * @brief 返回测试注入的 FreeRTOS 调度器状态。
 * @return g_test_scheduler_state 的值；0 表示未启动，1 表示运行中。
 * @post 不修改调度器、中断掩码或测试计数。
 * @note 这是 FreeRTOS API 桩，不查询真实调度器，也不会失败。
 */
uint32_t xTaskGetSchedulerState(void);

#endif //LAYERED_EMBEDDED_C_TEST_FREERTOS_H
