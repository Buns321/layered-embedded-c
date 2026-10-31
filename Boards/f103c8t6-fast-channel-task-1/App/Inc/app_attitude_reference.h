/**
 * @file app_attitude_reference.h
 * @brief 快速通道任务一直接姿态和陀螺积分参考。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_ATTITUDE_REFERENCE_H
#define LAYERED_EMBEDDED_C_APP_ATTITUDE_REFERENCE_H

#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 参考姿态快照。
 */
typedef struct app_attitude_reference_snapshot_s {
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
  /** @brief 最近参考更新使用的周期，单位 s。 */
  float dt_s;
} app_attitude_reference_snapshot_t;

/**
 * @brief 参考姿态计算状态。
 */
typedef struct app_attitude_reference_s {
  uint32_t last_time_ms;
  float last_dt_s;
  float direct_roll_rad;
  float direct_pitch_rad;
  float integrated_roll_rad;
  float integrated_pitch_rad;
  float integrated_yaw_rad;
  bool has_time;
  bool initialized;
} app_attitude_reference_t;

/**
 * @brief 初始化参考姿态状态。
 * @param reference_h 待初始化状态，不得为 NULL。
 * @param nominal_dt_s 首次更新周期，单位 s；必须有限且大于 0。
 * @retval true 状态已初始化。
 * @retval false 指针为空或周期非法。
 * @pre 只由单一 IMU 任务持有。
 * @post 所有姿态角和上一帧时间清零，last_dt_s 保存标称周期。
 * @note 不分配内存。
 */
bool App_AttitudeReference_Init(
  app_attitude_reference_t *reference_h,
  float nominal_dt_s
);

/**
 * @brief 使用一帧校正后 IMU 样本更新参考姿态。
 * @param reference_h 已初始化状态，不得为 NULL。
 * @param sample 非空校正后样本；加速度单位 m/s²，角速度单位 deg/s。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 单任务调用，样本 valid_mask 可只包含部分通道。
 * @post 三轴加速度均有效时更新直接姿态，三轴陀螺均有效时更新积分姿态；
 *       dt_s 夹紧到配置范围。
 * @note 不访问硬件、不分配内存；缺失通道对应角保持不变。
 */
void App_AttitudeReference_Update(
  app_attitude_reference_t *reference_h,
  const imu_sample_t *sample,
  uint32_t now_ms
);

/**
 * @brief 复制当前参考姿态快照。
 * @param reference_h 已初始化状态，不得为 NULL。
 * @param snapshot 输出快照，不得为 NULL。
 * @pre reference_h 只由单一写者更新，调用方接受瞬时快照。
 * @post snapshot 获得五轴参考角和最近 dt。
 * @note 不分配内存。
 */
void App_AttitudeReference_GetSnapshot(
  const app_attitude_reference_t *reference_h,
  app_attitude_reference_snapshot_t *snapshot
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_ATTITUDE_REFERENCE_H
