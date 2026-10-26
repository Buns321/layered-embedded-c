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
 * @brief 读取测试调度器状态。
 * @return 测试用调度器状态。
 */
uint32_t xTaskGetSchedulerState(void);

#endif //LAYERED_EMBEDDED_C_TEST_FREERTOS_H
