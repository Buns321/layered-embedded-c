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
 * @param actual 实际周期或时间戳，单位与 target 相同（当前为 RTOS tick）。
 * @param target 目标周期或时间戳，单位与 actual 相同。
 * @return 两者差值的绝对值，单位相同；结果不会因 uint32 回绕而变成负数。
 * @note 纯函数；适用于差值小于 2^32 的单调时间戳，不分配内存。
 */
static uint32_t rtos_app_absolutejitter(uint32_t actual, uint32_t target) {
  return actual >= target ? actual - target : target - actual;
}

/**
 * @brief 创建静态 ImuEkfTask。
 * @retval true ImuEkfTask 已用静态 TCB 和静态栈成功创建。
 * @retval false osThreadNew 返回 NULL，表示静态内存、优先级或内核属性不合法。
 * @pre osKernelInitialize() 已调用，且 App_FastChannel_Init 已完成所需的静态对象初始化。
 * @post 成功时任务处于 AboveNormal 优先级并可被调度；VofaTask 由 CubeMX 的 defaultTask 槽位创建。
 * @note 不分配动态内存；应在 osKernelStart 前调用一次。
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
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 仅由 CMSIS-RTOS2 任务调度器调用，且 App_FastChannel_Init 已成功返回。
 * @post 每次周期调用 App_FastChannel_ProcessImu，并通过 App_FastChannel_ReportImuMetrics 更新 ImuEkfTask 内部性能统计。
 * @note 任务以 osDelayUntil 维持 10 ms 周期；正常运行时永不返回，不分配内存。
 * @note 若延迟超过一个周期，下一次唤醒从当前 tick 重新排程，不补跑历史周期。
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
 * @param argument CMSIS-RTOS2 任务参数；当前忽略，允许为 NULL。
 * @pre 当前由 CubeMX 生成的 defaultTask 槽位调用，且 App_FastChannel_Init 已成功返回。
 * @post 每次周期调用 App_FastChannel_ProcessTelemetry，完成命令消费和 32 通道 JustFloat 发送。
 * @note 任务以 osDelayUntil 维持 20 ms 周期；正常运行时永不返回，不分配内存。
 * @note 若延迟超过一个周期，下一次唤醒从当前 tick 重新排程，不补跑历史周期。
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
