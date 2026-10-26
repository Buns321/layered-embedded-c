//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file filter_imu_calibration.h
 * @brief 统一 IMU 六轴数据的零偏与比例校正滤波器。
 * @addtogroup filter
 * @{
 * @note 输入和输出沿用 IMU 固定通道 schema：陀螺单位为 deg/s，加速度单位为
 *       m/s^2。校正只修改陀螺 XYZ 与加速度 XYZ，不增加姿态通道。
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_IMU_CALIBRATION_H
#define LAYERED_EMBEDDED_C_FILTER_IMU_CALIBRATION_H

#include "filter_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief IMU 六轴校正参数。
 * @note accel_scale 是无量纲比例系数，正常情况下接近 1。
 * @note gyro_bias_dps 的单位为 deg/s，应用方式为 raw - bias。
 */
typedef struct filter_imu_calibration_config_s {
  /** @brief 三轴加速度零偏，单位 m/s²。 */
  float accel_offset_mps2[3];
  /** @brief 三轴加速度比例系数，无量纲，有效范围 0.1..10。 */
  float accel_scale[3];
  /** @brief 三轴陀螺零偏，单位 deg/s，绝对值不得超过 720。 */
  float gyro_bias_dps[3];
} filter_imu_calibration_config_t;

/**
 * @brief IMU 六轴校正滤波器对象。
 * @note filter_base 必须是首成员。
 */
typedef struct filter_imu_calibration_s {
  filter_base_t filter_base;
  filter_imu_calibration_config_t config;
  uint32_t corrected_frames;
} filter_imu_calibration_t;

/**
 * @brief 初始化六轴校正滤波器。
 * @param filter_h 对象。
 * @param name 名称。
 * @param config 初始校正参数，不能为空。
 * @retval true 初始化和参数校验成功。
 * @retval false 参数为空、非有限或比例系数超出安全范围。
 * @note 只允许在任务初始化阶段调用。
 */
bool Filter_ImuCalibration_Init(
  filter_imu_calibration_t *filter_h,
  const char *name,
  const filter_imu_calibration_config_t *config
);

/**
 * @brief 在帧边界更新六轴校正参数。
 * @param filter_h 对象。
 * @param config 新参数。
 * @retval true 参数有效且已提交。
 * @retval false 参数无效，原配置保持不变。
 * @note 可以从另一任务调用；配置只在一次完整 update 前复制并应用。
 * @pre filter_h 和 config 非空。
 */
bool Filter_ImuCalibration_SetConfig(
  filter_imu_calibration_t *filter_h,
  const filter_imu_calibration_config_t *config
);

/**
 * @brief 复制当前六轴校正参数。
 * @param filter_h 对象。
 * @param config 输出参数。
 * @pre filter_h 和 config 非空。
 */
void Filter_ImuCalibration_GetConfig(
  const filter_imu_calibration_t *filter_h,
  filter_imu_calibration_config_t *config
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_IMU_CALIBRATION_H
