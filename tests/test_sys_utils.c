/**
 * @file test_sys_utils.c
 * @brief Framework/Core 基础工具模块宿主机单元测试。
 * @ingroup tests
 */

#include "bsp_mcu.h"
#include "rtos_task.h"
#include "sys_angle.h"
#include "sys_byte_ring.h"
#include "sys_critical.h"
#include "sys_fault.h"
#include "sys_log.h"
#include "sys_math.h"
#include "sys_snapshot.h"
#include "sys_text.h"
#include "sys_time_window.h"
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool near_f32(float left, float right) { return fabsf(left - right) <= 1e-5f; }

static int s_eval_count;

static int inc_and_get(int v) {
  s_eval_count++;
  return v;
}

static void test_sys_math(void) {
  s_eval_count = 0;
  assert(SYS_ABS(inc_and_get(-42)) == 42);
  assert(s_eval_count == 1);
  assert(SYS_ABS(INT_MIN) == INT_MAX);
  assert(SYS_ABS(LONG_MIN) == LONG_MAX);
  assert(SYS_ABS(7U) == 7U);
  assert(near_f32(SYS_ABS(-3.5f), 3.5f));
  assert(fabs(SYS_ABS(-2.25) - 2.25) < 1e-9);

  s_eval_count = 0;
  assert(SYS_MIN(inc_and_get(3), inc_and_get(5)) == 3);
  assert(SYS_MAX(inc_and_get(3), inc_and_get(5)) == 5);
  assert(s_eval_count == 4);

  assert(SYS_CLAMP(-10, -5, 5) == -5);
  assert(SYS_CLAMP(10, -5, 5) == 5);
  assert(SYS_CLAMP(2, -5, 5) == 2);
  assert(near_f32(SYS_CLAMP(1.5f, -1.0f, 1.0f), 1.0f));

  assert(SYS_SIGN(-9) == -1);
  assert(SYS_SIGN(0) == 0);
  assert(SYS_SIGN(9) == 1);
  assert(SYS_SIGN(0U) == 0U);
  assert(SYS_SIGN(9U) == 1U);
  assert(near_f32(SYS_SIGN(-0.1f), -1.0f));

  assert(SYS_ISNAN(NAN));
  assert(!SYS_ISNAN(1.0f));
  assert(SYS_ISINF(INFINITY));
  assert(SYS_ISINF(-INFINITY));
  assert(!SYS_ISINF(NAN));
  assert(SYS_ISFINITE(0.0f));
  assert(!SYS_ISFINITE(NAN));
  assert(!SYS_ISFINITE(INFINITY));
  assert(SYS_IS_POS_FINITE(0.1f));
  assert(!SYS_IS_POS_FINITE(0.0f));
  assert(!SYS_IS_POS_FINITE(INFINITY));
  assert(SYS_IS_NONNEG_FINITE(0.0));
  assert(!SYS_IS_NONNEG_FINITE(-0.001));
}

static void test_sys_angle(void) {
  float value = 7.0f;
  assert(Sys_Angle_WrapDegrees(540.0f, &value));
  assert(near_f32(value, 180.0f));
  assert(Sys_Angle_WrapDegrees(-540.0f, &value));
  assert(near_f32(value, -180.0f));
  assert(Sys_Angle_WrapDegrees(190.0f, &value));
  assert(near_f32(value, -170.0f));

  value = 7.0f;
  assert(Sys_Angle_WrapRadians(3.0f * SYS_PI, &value));
  assert(near_f32(value, SYS_PI));
  assert(Sys_Angle_WrapRadians(-3.0f * SYS_PI, &value));
  assert(near_f32(value, -SYS_PI));

  value = 7.0f;
  assert(!Sys_Angle_WrapDegrees(NAN, &value));
  assert(value == 7.0f);
  assert(!Sys_Angle_WrapRadians(INFINITY, &value));
  assert(value == 7.0f);
  assert(!Sys_Angle_WrapDegrees(0.0f, NULL));
  assert(!Sys_Angle_WrapRadians(0.0f, NULL));

  assert(near_f32(Sys_Angle_DegToRad(180.0f), SYS_PI));
  assert(near_f32(Sys_Angle_DegToRad(-90.0f), -SYS_PI * 0.5f));
  assert(near_f32(Sys_Angle_RadToDeg(SYS_PI), 180.0f));
}

static void test_sys_time_window(void) {
  assert(Sys_Time_HasReachedMs(100U, 100U));
  assert(Sys_Time_HasReachedMs(101U, 100U));
  assert(!Sys_Time_HasReachedMs(99U, 100U));
  assert(Sys_Time_HasReachedMs(0U, UINT32_MAX));

  assert(Sys_Time_HasElapsedMs(100U, 100U, 0U));
  assert(Sys_Time_HasElapsedMs(110U, 100U, 10U));
  assert(!Sys_Time_HasElapsedMs(109U, 100U, 10U));
  assert(Sys_Time_HasElapsedMs(5U, UINT32_MAX - 4U, 10U));

  assert(Sys_Time_IsAfterMs(101U, 100U));
  assert(!Sys_Time_IsAfterMs(100U, 100U));
  assert(!Sys_Time_IsAfterMs(99U, 100U));
  assert(Sys_Time_IsAfterMs(0U, UINT32_MAX));
  assert(!Sys_Time_IsAfterMs(UINT32_MAX, 0U));

  assert(Sys_Time_ElapsedMs(100U, 125U) == 25U);
  assert(Sys_Time_ElapsedMs(UINT32_MAX - 4U, 5U) == 10U);

  assert(!Sys_Time_IsExpiredMs(110U, 100U, 10U));
  assert(Sys_Time_IsExpiredMs(111U, 100U, 10U));
  assert(Sys_Time_IsExpiredMs(6U, UINT32_MAX - 4U, 10U));

  uint32_t last_ms = 0U;
  assert(Sys_Time_TryStepPeriodicMs(&last_ms, 50U, 100U));
  assert(last_ms == 50U);
  assert(!Sys_Time_TryStepPeriodicMs(&last_ms, 149U, 100U));
  assert(last_ms == 50U);
  assert(Sys_Time_TryStepPeriodicMs(&last_ms, 150U, 100U));
  assert(last_ms == 150U);
  assert(!Sys_Time_TryStepPeriodicMs(NULL, 200U, 100U));
}

static void test_sys_byte_ring(void) {
  uint8_t buffer[4];
  sys_byte_ring_t ring;
  assert(Sys_ByteRing_Init(&ring, buffer, sizeof(buffer)));
  assert(Sys_ByteRing_IsEmpty(&ring));

  assert(Sys_ByteRing_PushFromISR(&ring, 1U));
  assert(Sys_ByteRing_PushFromISR(&ring, 2U));
  assert(Sys_ByteRing_PushFromISR(&ring, 3U));
  assert(!Sys_ByteRing_PushFromISR(&ring, 4U));
  assert(Sys_ByteRing_GetDroppedBytes(&ring) == 1U);

  uint8_t value = 0U;
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 1U);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 2U);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 3U);
  assert(!Sys_ByteRing_Pop(&ring, &value));

  assert(Sys_ByteRing_PushFromISR(&ring, 4U));
  assert(Sys_ByteRing_PushFromISR(&ring, 5U));
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 4U);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 5U);

  assert(!Sys_ByteRing_Init(NULL, buffer, sizeof(buffer)));
  assert(!Sys_ByteRing_Init(&ring, NULL, sizeof(buffer)));
  assert(!Sys_ByteRing_Init(&ring, buffer, 1U));
  assert(!Sys_ByteRing_Init(&ring, buffer, 3U));
  assert(Sys_ByteRing_IsEmpty(NULL));
  assert(Sys_ByteRing_GetDroppedBytes(NULL) == 0U);
}

static void test_sys_text(void) {
  assert(Sys_Text_EqualsIgnoreCase("Reset", "RESET"));
  assert(Sys_Text_EqualsIgnoreCase("", ""));
  assert(!Sys_Text_EqualsIgnoreCase("SET", "SETTING"));
  assert(!Sys_Text_EqualsIgnoreCase(NULL, "SET"));

  char line[] = " \tSET\r  Q_ANGLE\t1.25e-4  ";
  char *tokens[3];
  uint32_t token_count = 0U;
  assert(Sys_Text_SplitWhitespace(line, tokens, 3U, &token_count));
  assert(token_count == 3U);
  assert(strcmp(tokens[0], "SET") == 0);
  assert(strcmp(tokens[1], "Q_ANGLE") == 0);
  assert(strcmp(tokens[2], "1.25e-4") == 0);

  char blank[] = " \t\r";
  assert(!Sys_Text_SplitWhitespace(blank, tokens, 3U, &token_count));
  char extra[] = "SET A 1 extra";
  assert(!Sys_Text_SplitWhitespace(extra, tokens, 3U, &token_count));

  float value = 42.0f;
  assert(Sys_Text_ParseFiniteF32("1.25e-4", &value));
  assert(fabsf(value - 0.000125f) < 1e-9f);
  assert(Sys_Text_ParseFiniteF32("-0.5", &value));
  assert(near_f32(value, -0.5f));
  value = 42.0f;
  assert(!Sys_Text_ParseFiniteF32("1x", &value));
  assert(value == 42.0f);
  assert(!Sys_Text_ParseFiniteF32("nan", &value));
  assert(!Sys_Text_ParseFiniteF32("1e1000000", &value));
}

typedef struct test_snap_s {
  uint32_t a;
  float b;
} test_snap_t;

static uint32_t s_fault_hook_calls;
static sys_fault_info_t s_fault_hook_info;

void Sys_Fault_OnFault(const sys_fault_info_t *info) {
  s_fault_hook_calls++;
  if (info != NULL) {
    s_fault_hook_info = *info;
  }
}

static void test_sys_critical_snapshot_and_fault(void) {
  BSP_Stub_ResetAll();
  __set_PRIMASK(0U);

  Sys_Critical_Enter();
  assert(__get_PRIMASK() == 1U);
  Sys_Critical_Enter();
  assert(__get_PRIMASK() == 1U);
  Sys_Critical_Exit();
  assert(__get_PRIMASK() == 1U);
  Sys_Critical_Exit();
  assert(__get_PRIMASK() == 0U);

  __set_PRIMASK(1U);
  Sys_Critical_Enter();
  assert(__get_PRIMASK() == 1U);
  Sys_Critical_Exit();
  assert(__get_PRIMASK() == 1U);
  __set_PRIMASK(0U);

  sys_critical_state_t isr_state = Sys_Critical_EnterFromISR();
  assert(isr_state == 0U);
  assert(__get_PRIMASK() == 1U);
  Sys_Critical_ExitFromISR(isr_state);
  assert(__get_PRIMASK() == 0U);

  test_snap_t shared = {0U, 0.0f};
  test_snap_t src = {123U, 4.5f};
  test_snap_t dst = {0U, 0.0f};
  SYS_SNAPSHOT_WRITE(&shared, &src);
  SYS_SNAPSHOT_READ(&dst, &shared);
  assert(dst.a == 123U && near_f32(dst.b, 4.5f));

  s_fault_hook_calls = 0U;
  SCB->CFSR = 0x01020304U;
  SCB->HFSR = 0x40000000U;
  SCB->MMFAR = 0x11223344U;
  SCB->BFAR = 0x55667788U;
  BSP_Stub_SetCoreRegs(0x2001FF00U, 0x20008000U);
  Sys_Fault_Handler(SYS_FAULT_BUS);

  assert(s_fault_hook_calls == 1U);
  assert(__get_PRIMASK() == 1U);
  const sys_fault_info_t *info = Sys_Fault_GetInfo();
  assert(info->type == SYS_FAULT_BUS);
  assert(info->cfsr == 0x01020304U);
  assert(info->hfsr == 0x40000000U);
  assert(info->mmfar == 0x11223344U);
  assert(info->bfar == 0x55667788U);
  assert(info->msp == 0x2001FF00U);
  assert(info->psp == 0x20008000U);
  assert(s_fault_hook_info.bfar == 0x55667788U);
  __set_PRIMASK(0U);
}

#define TEST_SINK_CAPACITY 512U

typedef struct test_log_sink_s {
  sys_log_base_t base;
  uint8_t data[TEST_SINK_CAPACITY];
  uint32_t size;
  uint32_t accepted_limit;
  uint32_t write_calls;
  bool trigger_reentrant;
} test_log_sink_t;

static uint32_t test_log_write(sys_log_base_t *log_h, const uint8_t *data, uint32_t size) {
  test_log_sink_t *sink = (test_log_sink_t *)log_h;
  uint32_t accepted = (size < sink->accepted_limit) ? size : sink->accepted_limit;
  uint32_t available = TEST_SINK_CAPACITY - sink->size;
  if (accepted > available) {
    accepted = available;
  }
  if (accepted > 0U) {
    (void)memcpy(&sink->data[sink->size], data, accepted);
    sink->size += accepted;
  }
  sink->write_calls++;
  if (sink->trigger_reentrant) {
    sink->trigger_reentrant = false;
    SYS_LOG_WARN("NESTED", "blocked");
  }
  return accepted;
}

static const sys_log_ops_t s_test_log_ops = {
    .write = test_log_write,
};

static void test_log_sink_reset(test_log_sink_t *sink) {
  (void)memset(sink, 0, sizeof(*sink));
  Sys_Log_Base_Init(&sink->base, "test", &s_test_log_ops);
  sink->accepted_limit = TEST_SINK_CAPACITY;
}

static int s_lazy_eval_count;

static int lazy_arg(void) {
  s_lazy_eval_count++;
  return 7;
}

static void test_sys_log(void) {
  BSP_Stub_ResetAll();
  test_log_sink_t sink;
  test_log_sink_reset(&sink);

  Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
  Sys_Log_Init(NULL);
  assert(!Sys_Log_IsReady());
  assert(!Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR));
  SYS_LOG_ERROR("TEST", "disabled");
  assert(sink.write_calls == 0U);

  Sys_Log_Init(&sink.base);
  assert(Sys_Log_IsReady());
  assert(Sys_Log_GetLevel() == SYS_LOG_LEVEL_INFO);

  s_lazy_eval_count = 0;
  SYS_LOG_DEBUG("TEST", "val=%d", lazy_arg());
  assert(s_lazy_eval_count == 0);
  assert(sink.write_calls == 0U);

  BSP_Stub_SetTickMs(1234U);
  SYS_LOG_INFO("APP", "value=%d", 42);
  const char *expected = "[1234][I][APP] value=42\r\n";
  assert(sink.write_calls == 1U);
  assert(sink.size == strlen(expected));
  assert(memcmp(sink.data, expected, sink.size) == 0);

  test_log_sink_reset(&sink);
  sink.accepted_limit = 5U;
  BSP_Stub_SetTickMs(10U);
  Sys_Log_Init(&sink.base);
  SYS_LOG_ERROR("TEST", "x");
  assert(sink.size == 5U);
  assert(Sys_Log_GetDropBytes() == (uint32_t)strlen("[10][E][TEST] x\r\n") - 5U);

  test_log_sink_reset(&sink);
  sink.trigger_reentrant = true;
  Sys_Log_Init(&sink.base);
  SYS_LOG_INFO("OUTER", "msg");
  assert(sink.write_calls == 1U);
  assert(Sys_Log_GetContentionCount() == 1U);

  test_log_sink_reset(&sink);
  Sys_Log_Init(&sink.base);
  char long_msg[300];
  (void)memset(long_msg, 'A', sizeof(long_msg) - 1U);
  long_msg[sizeof(long_msg) - 1U] = '\0';
  SYS_LOG_ERROR("TEST", "%s", long_msg);
  assert(sink.size == 191U);
  assert(sink.data[sink.size - 2U] == '\r');
  assert(sink.data[sink.size - 1U] == '\n');

  test_log_sink_reset(&sink);
  Sys_Log_Init(&sink.base);
  const uint8_t raw[] = {0x11U, 0x00U, 0xFEU};
  Sys_Log_EmergencyRaw(raw, sizeof(raw));
  assert(sink.size == sizeof(raw));
  assert(memcmp(sink.data, raw, sizeof(raw)) == 0);

  Sys_Log_SetLevel((sys_log_level_t)99);
  assert(Sys_Log_GetLevel() == SYS_LOG_LEVEL_NONE);
  Sys_Log_Write((sys_log_level_t)99, "TEST", "ignored");
  Sys_Log_Write(SYS_LOG_LEVEL_ERROR, NULL, "ignored");
  assert(sink.write_calls == 1U);
}

typedef struct test_rtos_sim_s {
  uint32_t step_calls;
  uint32_t now_ms_history[4];
  uint32_t halt_hook_calls;
} test_rtos_sim_t;

static test_rtos_sim_t s_rtos_sim;

static void test_rtos_step(uint32_t now_ms) {
  uint32_t idx = s_rtos_sim.step_calls;
  if (idx < 4U) {
    s_rtos_sim.now_ms_history[idx] = now_ms;
  }
  s_rtos_sim.step_calls++;

  if (idx == 0U) {
    DWT->CYCCNT += 168U * 250U;
    RTOS_Stub_SetDelayUntilStatus(osOK);
  } else if (idx == 1U) {
    /* 模拟执行耗时超出 10 ms 周期且 osDelayUntil 检测到已错过唤醒 tick。 */
    DWT->CYCCNT += 168U * 12000U;
    RTOS_Stub_SetTickCount(122U);
    RTOS_Stub_SetDelayUntilStatus(osErrorParameter);
  } else if (idx == 2U) {
    DWT->CYCCNT += 168U * 100U;
    RTOS_Stub_SetDelayUntilStatus(osOK);
  } else {
    DWT->CYCCNT += 168U * 50U;
    RTOS_Stub_SetDelayUntilStatus(osOK);
  }
}

static osStatus_t test_rtos_delay_until_preempt(uint32_t ticks) {
  if (ticks == 120U) {
    return osErrorParameter;
  }
  /* 第 3 步（ticks == 132）模拟任务被高优先级抢占延后至 1100U 才醒来。 */
  if (ticks == 132U) {
    RTOS_Stub_SetTickCount(1100U);
    RTOS_Stub_SetStackSpace(384U);
  } else {
    RTOS_Stub_SetTickCount(ticks);
  }
  return osOK;
}

static void test_rtos_step_200hz(uint32_t now_ms) {
  s_rtos_sim.now_ms_history[0] = now_ms;
  s_rtos_sim.step_calls++;
}

static void test_rtos_step_delay_fail(uint32_t now_ms) {
  (void)now_ms;
  DWT->CYCCNT += 168U * 20U;
  RTOS_Stub_SetTickCount(65U);
  RTOS_Stub_SetDelayUntilStatus(osErrorParameter);
  s_rtos_sim.step_calls++;
}

static void test_rtos_halt_hook(void) { s_rtos_sim.halt_hook_calls++; }

RTOS_TASK_PERIODIC(s_test_periodic_task, 10U, osPriorityAboveNormal, 255U, test_rtos_step);
RTOS_TASK_PERIODIC(s_test_200hz_task, 12U, osPriorityNormal, 128U, test_rtos_step_200hz);
RTOS_TASK_PERIODIC(s_test_delay_fail_task, 10U, osPriorityNormal, 128U, test_rtos_step_delay_fail);

static void test_rtos_task(void) {
  BSP_Stub_ResetAll();
  s_rtos_sim = (test_rtos_sim_t){.step_calls = 0U};

  assert(strcmp(s_test_periodic_task.name, "s_test_periodic_task") == 0);
  assert(s_test_periodic_task.period_ms == 10U);
  assert(s_test_periodic_task.priority == osPriorityAboveNormal);
  assert(s_test_periodic_task.stack_bytes == 1024U);
  assert(((uintptr_t)s_test_periodic_task.stack_buf % 8U) == 0U);

  rtos_periodic_task_t invalid_task = s_test_periodic_task;
  assert(!RTOS_Task_StartPeriodic(NULL));
  invalid_task.step_fn = NULL;
  assert(!RTOS_Task_StartPeriodic(&invalid_task));
  invalid_task = s_test_periodic_task;
  invalid_task.period_ms = 0U;
  assert(!RTOS_Task_StartPeriodic(&invalid_task));
  invalid_task = s_test_periodic_task;
  invalid_task.stack_buf = NULL;
  assert(!RTOS_Task_StartPeriodic(&invalid_task));
  invalid_task = s_test_periodic_task;
  invalid_task.stack_bytes = 0U;
  assert(!RTOS_Task_StartPeriodic(&invalid_task));

  s_test_periodic_task.handle = NULL;
  RTOS_Stub_SetThreadNewFail(true);
  assert(!RTOS_Task_StartPeriodic(&s_test_periodic_task));
  assert(s_test_periodic_task.handle == NULL);

  RTOS_Stub_SetThreadNewFail(false);
  assert(RTOS_Task_StartPeriodic(&s_test_periodic_task));
  assert(s_test_periodic_task.handle != NULL);
  assert(!RTOS_Task_StartPeriodic(&s_test_periodic_task));

  const osThreadAttr_t *attr = RTOS_Stub_GetLastThreadAttr();
  assert(attr != NULL);
  assert(attr->cb_mem == &s_test_periodic_task.tcb);
  assert(attr->cb_size == sizeof(StaticTask_t));
  assert(attr->stack_mem == s_test_periodic_task.stack_buf);
  assert(attr->stack_size == s_test_periodic_task.stack_bytes);
  assert(attr->priority == osPriorityAboveNormal);
  assert(strcmp(attr->name, "s_test_periodic_task") == 0);
  assert(RTOS_Stub_GetLastThreadArg() == &s_test_periodic_task);

  /* 驱动 4 个周期：正常 -> 超时重对齐 -> 正常 + 抢占跳变 -> 跨 1000ms 重采栈水位并重对齐。 */
  RTOS_Stub_SetTickFreq(1000U);
  RTOS_Stub_SetTickCount(100U);
  RTOS_Stub_SetStackSpace(512U);
  RTOS_Stub_SetMaxLoopIterations(4U);
  RTOS_Stub_SetDelayUntilHandler(NULL);

  osThreadFunc_t entry = RTOS_Stub_GetLastThreadFunc();
  assert(entry != NULL);

  RTOS_Stub_SetMaxLoopIterations(3U);
  entry(RTOS_Stub_GetLastThreadArg());
  assert(s_rtos_sim.step_calls == 3U);

  rtos_task_stats_t stats = {.last_exec_us = 0U};
  RTOS_Task_GetStats(&s_test_periodic_task, &stats);
  assert(s_rtos_sim.now_ms_history[0] == 100U);
  assert(s_rtos_sim.now_ms_history[1] == 110U);
  assert(s_rtos_sim.now_ms_history[2] == 122U);
  assert(stats.last_exec_us == 100U);
  assert(stats.max_exec_us == 12000U);
  assert(stats.overrun_count == 1U);
  assert(stats.stack_free_bytes == 512U);
  assert(RTOS_Stub_GetStackSpaceCalls() == 1U);

  /* 单独验证跨 1000ms 抢占唤醒触发 missed_period 与重新采样栈水位。 */
  BSP_Stub_ResetAll();
  s_rtos_sim = (test_rtos_sim_t){.step_calls = 0U};
  s_test_periodic_task.handle = NULL;
  RTOS_Stub_SetTickFreq(1000U);
  RTOS_Stub_SetTickCount(100U);
  RTOS_Stub_SetStackSpace(512U);
  assert(RTOS_Task_StartPeriodic(&s_test_periodic_task));
  RTOS_Stub_SetMaxLoopIterations(4U);
  RTOS_Stub_SetDelayUntilHandler(test_rtos_delay_until_preempt);
  RTOS_Stub_GetLastThreadFunc()(RTOS_Stub_GetLastThreadArg());

  RTOS_Task_GetStats(&s_test_periodic_task, &stats);
  assert(s_rtos_sim.step_calls == 4U);
  assert(s_rtos_sim.now_ms_history[3] == 1100U);
  assert(RTOS_Stub_GetLastDelayUntilTick() == 1110U);
  assert(stats.last_exec_us == 50U);
  assert(stats.max_exec_us == 12000U);
  assert(stats.overrun_count == 2U);
  assert(stats.stack_free_bytes == 384U);
  assert(RTOS_Stub_GetStackSpaceCalls() == 2U);

  /* 验证非 1000 Hz tick 频率换算（200 Hz 下 12 ms 向上取整为 3 ticks）。 */
  BSP_Stub_ResetAll();
  s_rtos_sim = (test_rtos_sim_t){.step_calls = 0U};
  s_test_200hz_task.handle = NULL;
  RTOS_Stub_SetTickFreq(200U);
  RTOS_Stub_SetTickCount(50U);
  assert(RTOS_Task_StartPeriodic(&s_test_200hz_task));
  RTOS_Stub_SetMaxLoopIterations(1U);
  RTOS_Stub_GetLastThreadFunc()(RTOS_Stub_GetLastThreadArg());
  assert(s_rtos_sim.now_ms_history[0] == 250U);
  assert(RTOS_Stub_GetLastDelayUntilTick() == 53U);

  /* 验证步内耗时未超周期但因抢占导致 osDelayUntil 失败时的超时计数与对齐。 */
  BSP_Stub_ResetAll();
  s_rtos_sim = (test_rtos_sim_t){.step_calls = 0U};
  s_test_delay_fail_task.handle = NULL;
  RTOS_Stub_SetTickFreq(1000U);
  RTOS_Stub_SetTickCount(50U);
  assert(RTOS_Task_StartPeriodic(&s_test_delay_fail_task));
  RTOS_Stub_SetMaxLoopIterations(1U);
  RTOS_Stub_GetLastThreadFunc()(RTOS_Stub_GetLastThreadArg());
  RTOS_Task_GetStats(&s_test_delay_fail_task, &stats);
  assert(s_rtos_sim.step_calls == 1U);
  assert(stats.last_exec_us == 20U);
  assert(stats.overrun_count == 1U);

  /* 验证栈溢出安全钩子：关中断 + 调用紧急停机回调 + 应急日志输出。 */
  BSP_Stub_ResetAll();
  test_log_sink_t sink;
  test_log_sink_reset(&sink);
  Sys_Log_Init(&sink.base);
  RTOS_SetEmergencyHaltHook(test_rtos_halt_hook);
  __set_PRIMASK(0U);

  vApplicationStackOverflowHook((TaskHandle_t)&s_test_periodic_task, "CtrlTask");
  assert(__get_PRIMASK() == 1U);
  assert(s_rtos_sim.halt_hook_calls == 1U);
  const char *overflow_msg = "[RTOS] stack overflow: CtrlTask\r\n";
  assert(sink.size == strlen(overflow_msg));
  assert(memcmp(sink.data, overflow_msg, sink.size) == 0);

  test_log_sink_reset(&sink);
  Sys_Log_Init(&sink.base);
  vApplicationStackOverflowHook((TaskHandle_t)&s_test_periodic_task, NULL);
  const char *unknown_msg = "[RTOS] stack overflow: unknown\r\n";
  assert(sink.size == strlen(unknown_msg));
  assert(memcmp(sink.data, unknown_msg, sink.size) == 0);

  RTOS_SetEmergencyHaltHook(NULL);
  Sys_Log_Init(NULL);
  __set_PRIMASK(0U);
}

int main(void) {
  test_sys_math();
  test_sys_angle();
  test_sys_time_window();
  test_sys_byte_ring();
  test_sys_text();
  test_sys_critical_snapshot_and_fault();
  test_sys_log();
  test_rtos_task();
  (void)puts("test_sys_utils passed");
  return 0;
}
