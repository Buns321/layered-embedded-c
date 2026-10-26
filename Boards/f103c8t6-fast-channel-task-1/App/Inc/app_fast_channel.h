//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_fast_channel.h
 * @brief F103 速通赛道任务一应用入口、状态和一致快照。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H
#define LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H

#include "app_calibration.h"
#include "app_uart_link.h"
#include "filter_ekf.h"
#include "imu_base.h"
#include "mpu6050_i2c.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 应用主状态。
 */
typedef enum app_fast_channel_state_e {
  APP_FAST_CHANNEL_STATE_UNINITIALIZED = 0U,
  APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK,
  APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION,
  APP_FAST_CHANNEL_STATE_WAIT_FACE,
  APP_FAST_CHANNEL_STATE_STABILIZE_FACE,
  APP_FAST_CHANNEL_STATE_SAMPLE_FACE,
  APP_FAST_CHANNEL_STATE_MAIN,
  APP_FAST_CHANNEL_STATE_ERROR,
} app_fast_channel_state_t;

/**
 * @brief 任务耗时和周期统计。
 */
typedef struct app_task_metrics_s {
  /** @brief 单次 ImuEkfTask 执行时间，单位 us。 */
  uint32_t execution_us;
  /** @brief 相对 10 ms 目标周期的绝对抖动，单位 ms。 */
  uint32_t period_jitter_ms;
  /** @brief 当前任务剩余栈空间，单位 byte。 */
  uint32_t stack_free_bytes;
} app_task_metrics_t;

/**
 * @brief 遥测任务使用的一致数据快照。
 */
typedef struct app_fast_channel_snapshot_s {
  /** @brief 未经应用层校正的最近原始样本。 */
  mpu6050_raw_sample_t raw_sample;
  /** @brief 经 Filter Chain 处理并包含 EKF 姿态的样本。 */
  imu_sample_t calibrated_sample;
  /** @brief EKF 最近一次状态快照，内部角度单位为 rad。 */
  filter_ekf_state_t ekf_state;
  /** @brief 当前标定状态机状态。 */
  app_fast_channel_state_t calibration_state;
  /** @brief 已接受六面的位掩码。 */
  uint32_t face_mask;
  /** @brief 正负样本齐全的三轴位掩码。 */
  uint32_t completed_axis_mask;
  /** @brief 最近一条参数命令结果。 */
  uint32_t command_status;
  /** @brief IMU 发布快照的递增序号。 */
  uint32_t sequence;
  /** @brief 加速度直接解算横滚角，单位 rad。 */
  float direct_roll_rad;
  /** @brief 加速度直接解算俯仰角，单位 rad。 */
  float direct_pitch_rad;
  /** @brief 陀螺积分横滚角，单位 rad。 */
  float integrated_roll_rad;
  /** @brief 陀螺积分俯仰角，单位 rad。 */
  float integrated_pitch_rad;
  /** @brief 陀螺积分偏航角，单位 rad。 */
  float integrated_yaw_rad;
  /** @brief 最近一次 EKF 更新使用的实际周期，单位 s。 */
  float dt_s;
  /** @brief 原始样本是否有效。 */
  bool raw_valid;
  /** @brief 滤波后样本是否有效。 */
  bool calibrated_valid;
  /** @brief 是否成功取得过 Flash 校准记录。 */
  bool flash_record_valid;
  /** @brief 本次运行是否出现过 Flash 写入失败。 */
  bool flash_write_failed;
} app_fast_channel_snapshot_t;

/**
 * @brief 初始化板级设备、滤波链、Flash 校准和 UART 链路。
 * @retval PLATFORM_STATUS_OK 初始化成功。
 * @retval PLATFORM_STATUS_BUSY UART 回调已被其他对象占用。
 * @retval PLATFORM_STATUS_ERROR 滤波器、IMU 或底层外设初始化失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 板级配置不满足接口前置条件。
 * @pre CubeMX 已初始化 I2C1、USART1、DMA1_Channel4、GPIO 和 CRC。
 * @pre 只允许在 RTOS 调度器启动前调用一次。
 */
platform_status_t App_FastChannel_Init(void);

/**
 * @brief 执行一次 100 Hz IMU、标定和 EKF 处理。
 * @param now_ms 当前单调毫秒时间戳。
 * @pre 只允许 ImuEkfTask 调用。
 */
void App_FastChannel_ProcessImu(uint32_t now_ms);

/**
 * @brief 处理命令并发送 50 Hz 32 通道 JustFloat。
 * @param now_ms 当前单调毫秒时间戳。
 * @pre 只允许遥测任务调用。
 */
void App_FastChannel_ProcessTelemetry(uint32_t now_ms);

/**
 * @brief 复制最近一次一致快照。
 * @param snapshot 输出快照。
 * @pre snapshot 不能为空。
 */
void App_FastChannel_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
);

/**
 * @brief 更新 ImuEkfTask 性能统计。
 * @param metrics 最新性能统计。
 * @pre 只允许 ImuEkfTask 调用，metrics 不能为空。
 */
void App_FastChannel_ReportImuMetrics(
  const app_task_metrics_t *metrics
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_FAST_CHANNEL_H
