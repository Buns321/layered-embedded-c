/**
 * @file app_imu_session.h
 * @brief 快速通道任务一 IMU 管线和一致快照入口。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_IMU_SESSION_H
#define LAYERED_EMBEDDED_C_APP_IMU_SESSION_H

#include "app_calibration_flow.h"
#include "filter_ekf.h"
#include "filter_imu_calibration.h"
#include "imu_base.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief ImuEkfTask 耗时和周期统计。
 */
typedef struct app_task_metrics_s {
  /** @brief 单次任务执行时间，单位 us。 */
  uint32_t execution_us;
  /** @brief 相对 10 ms 目标周期的绝对抖动，单位 ms。 */
  uint32_t period_jitter_ms;
  /** @brief 当前任务剩余栈空间，单位 byte。 */
  uint32_t stack_free_bytes;
} app_task_metrics_t;

/**
 * @brief 遥测任务使用的 IMU 会话一致快照。
 */
typedef struct app_fast_channel_snapshot_s {
  /** @brief 未经应用层校正的最新原始样本。 */
  imu_sample_t raw_sample;
  /** @brief 经 Filter Chain 处理并包含 EKF 姿态的样本。 */
  imu_sample_t calibrated_sample;
  /** @brief EKF 最近一次状态快照，角度单位为 rad。 */
  filter_ekf_state_t ekf_state;
  /** @brief 当前标定状态机状态。 */
  app_fast_channel_state_t calibration_state;
  /** @brief 已接受六面的位掩码。 */
  uint32_t face_mask;
  /** @brief 正负样本齐全的三轴位掩码。 */
  uint32_t completed_axis_mask;
  /** @brief 快照递增序号。 */
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
  /** @brief 最近 EKF 更新使用的实际周期，单位 s。 */
  float dt_s;
  /** @brief 原始样本是否有效。 */
  bool raw_valid;
  /** @brief 滤波后样本是否有效。 */
  bool calibrated_valid;
  /** @brief 是否成功取得过 Flash 校准记录。 */
  bool flash_record_valid;
  /** @brief 本次运行是否出现过 Flash 写入失败。 */
  bool flash_write_failed;
  /** @brief 最近一次任务性能统计。 */
  app_task_metrics_t metrics;
} app_fast_channel_snapshot_t;

/**
 * @brief 初始化 IMU 记录、标定、提示、参考姿态和快照。
 * @param imu_h 已初始化并可提交样本的统一 IMU 句柄，不得为 NULL。
 * @param ekf_h 已初始化的 EKF 对象，不得为 NULL。
 * @param calibration_filter_h 已初始化的六轴校正滤波器，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 会话全部子模块已就绪。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 依赖指针为空。
 * @retval PLATFORM_STATUS_ERROR 默认记录或子模块初始化失败。
 * @pre 仅在调度器启动前、无并发使用者时调用。
 * @post 成功时进入初始姿态检查；Flash 缺失或损坏时使用默认记录继续。
 * @note 可能执行同步 Flash 扫描和擦除；不分配内存。
 */
platform_status_t App_ImuSession_Init(
  imu_base_t *imu_h,
  filter_ekf_t *ekf_h,
  filter_imu_calibration_t *calibration_filter_h
);

/**
 * @brief 推进一次 IMU 采样、标定流程、参考姿态和快照发布。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 会话已成功初始化，且只由单一 IMU 任务调用。
 * @post 已驱动 IMU process、推进标定、更新 EKF 重置和提示、更新参考姿态并发布快照。
 * @note 可能执行阻塞 I2C 和 Flash 操作；不分配内存。
 */
void App_ImuSession_Process(uint32_t now_ms);

/**
 * @brief 复制最近一次 IMU 会话快照。
 * @param snapshot 调用方输出存储，不得为 NULL。
 * @pre snapshot 可写且不与内部双缓冲重叠。
 * @post snapshot 获得一致的 IMU、EKF、标定、参考姿态和性能统计快照。
 * @note 使用短临界区复制；不分配内存，不得在 ISR 调用。
 */
void App_ImuSession_GetSnapshot(app_fast_channel_snapshot_t *snapshot);

/**
 * @brief 更新最近一次 IMU 任务性能统计。
 * @param metrics 非空性能统计；函数按值复制。
 * @pre 只由 ImuEkfTask 调用。
 * @post 后续快照包含新的性能统计。
 * @note 使用短临界区保护整体复制；不分配内存。
 */
void App_ImuSession_ReportMetrics(const app_task_metrics_t *metrics);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_IMU_SESSION_H
