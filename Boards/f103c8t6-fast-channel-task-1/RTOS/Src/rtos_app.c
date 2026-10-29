//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file rtos_app.c
 * @brief F103 任务一 RTOS 任务实现。
 * @ingroup rtos
 */

#include "rtos_app.h"
#include "FreeRTOS.h"
#include "app_fast_channel.h"
#include "app_fast_channel_config.h"
#include "cmsis_os2.h"
#include "system_log.h"
#include "system_time.h"
#include "task.h"
#include <stddef.h>
#include <stdint.h>

static StaticTask_t s_imu_task_cb;
static uint64_t s_imu_task_stack[
  APP_IMU_TASK_STACK_SIZE / sizeof(uint64_t)
];

/**
 * @brief 计算两个无符号时间戳的绝对差。
 */
static uint32_t rtos_app_absolutejitter(uint32_t actual, uint32_t target) {
  return actual >= target ? actual - target : target - actual;
}

/**
 * @brief 创建静态 ImuEkfTask。
 * @retval true 任务创建成功。
 * @retval false 静态内存或 CMSIS-RTOS2 属性非法。
 * @pre osKernelInitialize() 已调用。
 */
bool RTOS_App_CreateTasks(void) {
  const osThreadAttr_t imu_attributes = {
    .name = "ImuEkfTask",
    .attr_bits = osThreadDetached,
    .cb_mem = &s_imu_task_cb,
    .cb_size = sizeof(s_imu_task_cb),
    .stack_mem = s_imu_task_stack,
    .stack_size = sizeof(s_imu_task_stack),
    .priority = osPriorityAboveNormal,
  };

  return osThreadNew(
    RTOS_App_ImuTask,
    NULL,
    &imu_attributes
  ) != NULL;
}

/**
 * @brief 100 Hz IMU、标定和 EKF 任务入口。
 * @param argument CMSIS-RTOS2 参数，当前未使用。
 */
void RTOS_App_ImuTask(void *argument) {
  (void)argument;
  SYS_LOG_INFO("RTOS", "ImuEkfTask started");

  uint32_t next_wake_tick = osKernelGetTickCount();
  uint32_t last_tick = next_wake_tick;

  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    uint32_t start_cycles = Sys_Time_GetCycles();

    if (
      (uint32_t)(now_tick - next_wake_tick) >
      APP_IMU_TASK_PERIOD_MS
    ) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += APP_IMU_TASK_PERIOD_MS;

    App_FastChannel_ProcessImu(now_tick);

    uint32_t elapsed_cycles = Sys_Time_GetCycles() - start_cycles;
    uint32_t actual_period = now_tick - last_tick;
    last_tick = now_tick;

    app_task_metrics_t metrics = {
      .execution_us = Sys_Time_CyclesToUs(elapsed_cycles),
      .period_jitter_ms = rtos_app_absolutejitter(
        actual_period,
        APP_IMU_TASK_PERIOD_MS
      ),
      .stack_free_bytes = osThreadGetStackSpace(osThreadGetId()),
    };
    App_FastChannel_ReportImuMetrics(&metrics);

    (void)osDelayUntil(next_wake_tick);
  }
}

/**
 * @brief 50 Hz JustFloat 与参数命令任务入口。
 * @param argument CMSIS-RTOS2 参数，当前未使用。
 * @note 当前由 CubeMX 生成的 defaultTask 槽位调用。
 */
void RTOS_App_TelemetryTask(void *argument) {
  (void)argument;
  SYS_LOG_INFO("RTOS", "VofaTask started");

  uint32_t next_wake_tick = osKernelGetTickCount();
  for (;;) {
    uint32_t now_tick = osKernelGetTickCount();
    if (
      (uint32_t)(now_tick - next_wake_tick) >
      APP_TELEMETRY_TASK_PERIOD_MS
    ) {
      next_wake_tick = now_tick;
    }
    next_wake_tick += APP_TELEMETRY_TASK_PERIOD_MS;

    App_FastChannel_ProcessTelemetry(now_tick);
    (void)osDelayUntil(next_wake_tick);
  }
}
