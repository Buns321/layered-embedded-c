/**
 * @file app_attitude_reference.c
 * @brief 快速通道任务一直接姿态和陀螺积分参考实现。
 * @ingroup app
 */

#include "app_attitude_reference.h"
#include "app_fast_channel_config.h"
#include "system_angle.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define APP_ATTITUDE_DEG_TO_RAD 0.01745329251994329577f

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
) {
  if (
    reference_h == NULL ||
    !SYS_ISFINITE(nominal_dt_s) ||
    nominal_dt_s <= 0.0f
  ) {
    return false;
  }

  memset(reference_h, 0, sizeof(*reference_h));
  reference_h->last_dt_s = nominal_dt_s;
  reference_h->initialized = true;
  return true;
}

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
) {
  assert(reference_h && sample && reference_h->initialized);

  float dt_s = reference_h->last_dt_s;
  if (reference_h->has_time) {
    dt_s = (float)(now_ms - reference_h->last_time_ms) * 0.001f;
    dt_s = SYS_CLAMP(dt_s, APP_EKF_MIN_DT_S, APP_EKF_MAX_DT_S);
  }
  reference_h->last_time_ms = now_ms;
  reference_h->last_dt_s = dt_s;
  reference_h->has_time = true;

  if (
    (sample->valid_mask & IMU_CHANNEL_ACCEL_X) != 0u &&
    (sample->valid_mask & IMU_CHANNEL_ACCEL_Y) != 0u &&
    (sample->valid_mask & IMU_CHANNEL_ACCEL_Z) != 0u
  ) {
    float roll_rad = atan2f(
      sample->accel_mps2[1],
      sample->accel_mps2[2]
    );
    float pitch_rad = atan2f(
      -sample->accel_mps2[0],
      sqrtf(
        sample->accel_mps2[1] * sample->accel_mps2[1] +
        sample->accel_mps2[2] * sample->accel_mps2[2]
      )
    );
    if (SYS_ISFINITE(roll_rad) && SYS_ISFINITE(pitch_rad)) {
      reference_h->direct_roll_rad = roll_rad;
      reference_h->direct_pitch_rad = pitch_rad;
    }
  }

  if (
    (sample->valid_mask & IMU_CHANNEL_GYRO_X) != 0u &&
    (sample->valid_mask & IMU_CHANNEL_GYRO_Y) != 0u &&
    (sample->valid_mask & IMU_CHANNEL_GYRO_Z) != 0u
  ) {
    float roll_rad = 0.0f;
    float pitch_rad = 0.0f;
    float yaw_rad = 0.0f;
    if (Sys_Angle_WrapRadians(
          reference_h->integrated_roll_rad +
          sample->gyro_dps[0] * APP_ATTITUDE_DEG_TO_RAD * dt_s,
          &roll_rad
        )) {
      reference_h->integrated_roll_rad = roll_rad;
    }
    if (Sys_Angle_WrapRadians(
          reference_h->integrated_pitch_rad +
          sample->gyro_dps[1] * APP_ATTITUDE_DEG_TO_RAD * dt_s,
          &pitch_rad
        )) {
      reference_h->integrated_pitch_rad = pitch_rad;
    }
    if (Sys_Angle_WrapRadians(
          reference_h->integrated_yaw_rad +
          sample->gyro_dps[2] * APP_ATTITUDE_DEG_TO_RAD * dt_s,
          &yaw_rad
        )) {
      reference_h->integrated_yaw_rad = yaw_rad;
    }
  }
}

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
) {
  assert(reference_h && snapshot && reference_h->initialized);
  *snapshot = (app_attitude_reference_snapshot_t){
    .direct_roll_rad = reference_h->direct_roll_rad,
    .direct_pitch_rad = reference_h->direct_pitch_rad,
    .integrated_roll_rad = reference_h->integrated_roll_rad,
    .integrated_pitch_rad = reference_h->integrated_pitch_rad,
    .integrated_yaw_rad = reference_h->integrated_yaw_rad,
    .dt_s = reference_h->last_dt_s,
  };
}
