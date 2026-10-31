/**
 * @file app_fast_channel_telemetry_contract_test.c
 * @brief 快速通道任务一遥测映射契约测试。
 * @ingroup tests
 */

#include "app_fast_channel_telemetry.h"
#include "app_imu_session.h"
#include "system_log.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

volatile uint32_t g_sys_log_fast_mask;

static telemetry_sample_t s_last_sample;
static uint32_t s_send_calls;

/**
 * @brief 日志写入测试替身。
 * @param level 日志等级；测试忽略。
 * @param tag 标签；测试忽略。
 * @param format 格式字符串；测试忽略。
 * @param ... 可变参数；测试忽略。
 * @post 函数体为空。
 * @note 遥测测试不验证日志输出。
 */
void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
) {
  (void)level;
  (void)tag;
  (void)format;
}

/**
 * @brief 记录遥测发送样本的测试替身。
 * @param telemetry_h 遥测后端；测试不访问。
 * @param sample 非空样本指针；测试按值复制。
 * @return 固定返回 DEV_TELEMETRY_OK。
 * @pre sample 非空。
 * @post s_last_sample 更新且 s_send_calls 加一。
 * @note 不执行真实编码或 UART 发送。
 */
dev_telemetry_status_t Dev_Telemetry_Base_Send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  (void)telemetry_h;
  assert(sample);
  s_last_sample = *sample;
  s_send_calls++;
  return DEV_TELEMETRY_OK;
}

/**
 * @brief 遥测状态查询测试替身。
 * @param telemetry_h 遥测后端；测试不访问。
 * @param status 输出状态，不得为 NULL。
 * @post status 被清零。
 * @note 不访问真实协议后端。
 */
void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  (void)telemetry_h;
  assert(status);
  *status = (telemetry_status_t){0};
}

/**
 * @brief 验证 32 通道映射、占位姿态掩码和命令溢出覆盖。
 * @post 原始陀螺换算为 rad/s，EKF 占位通道无效，命令溢出状态写入通道 31。
 * @note 使用测试遥测后端，不访问硬件。
 */
static void test_mapping_and_placeholder(void) {
  app_fast_channel_telemetry_t telemetry = {0};
  App_FastChannelTelemetry_Init(&telemetry);

  app_fast_channel_snapshot_t snapshot = {0};
  snapshot.raw_valid = true;
  snapshot.calibrated_valid = true;
  snapshot.raw_sample.accel_mps2[0] = 1.0f;
  snapshot.raw_sample.gyro_dps[0] = 90.0f;
  snapshot.calibrated_sample.accel_mps2[0] = 2.0f;
  snapshot.calibrated_sample.gyro_dps[0] = 180.0f;
  snapshot.ekf_state.valid = true;
  snapshot.ekf_state.placeholder = true;
  snapshot.ekf_state.quaternion[0] = 1.0f;
  snapshot.calibration_state = APP_FAST_CHANNEL_STATE_MAIN;
  snapshot.face_mask = 0x3Fu;
  snapshot.dt_s = 0.01f;

  uart_link_status_t link_status = {
    .line_overflow_count = 1u,
  };
  App_FastChannelTelemetry_Process(
    &telemetry,
    (telemetry_base_t *)(uintptr_t)1u,
    &snapshot,
    &link_status,
    APP_FAST_CHANNEL_COMMAND_REJECTED,
    1000u
  );

  assert(s_send_calls == 1u);
  assert(fabsf(s_last_sample.value[0] - 1.0f) < 1e-6f);
  assert(fabsf(s_last_sample.value[3] - 2.0f) < 1e-6f);
  assert(
    fabsf(s_last_sample.value[6] - 1.57079632679f) < 1e-5f
  );
  assert(
    fabsf(s_last_sample.value[9] - 3.14159265359f) < 1e-5f
  );
  assert((s_last_sample.valid_mask & (0x7Fu << 17u)) == 0u);
  assert(
    s_last_sample.value[31] ==
    (float)APP_FAST_CHANNEL_COMMAND_OVERFLOW
  );
  assert(telemetry.observed_line_overflow_count == 1u);
}

/**
 * @brief 运行遥测映射契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试全局状态失效。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_mapping_and_placeholder();
  return 0;
}
