/**
 * @file task.h
 * @brief System 临界区契约测试使用的 FreeRTOS 任务 API 桩。
 * @ingroup tests
 */

#ifndef LAYERED_EMBEDDED_C_TEST_TASK_H
#define LAYERED_EMBEDDED_C_TEST_TASK_H

#include "FreeRTOS.h"

#define taskENTER_CRITICAL() \
  do { g_test_task_enter_count++; } while (0)

#define taskEXIT_CRITICAL() \
  do { g_test_task_exit_count++; } while (0)

#define taskENTER_CRITICAL_FROM_ISR() \
  (g_test_isr_enter_count++, g_test_primask)

#define taskEXIT_CRITICAL_FROM_ISR(state) \
  do { \
    (void)(state); \
    g_test_isr_exit_count++; \
  } while (0)

#endif //LAYERED_EMBEDDED_C_TEST_TASK_H
