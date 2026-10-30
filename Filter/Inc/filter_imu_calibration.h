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
 * @param filter_h 待初始化的校正滤波器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @param config 非空初始参数；加速度比例系数每轴须在 0.1..10，陀螺零偏绝对值不超过 720 deg/s。
 * @retval true 参数有效，对象已清零、绑定并完成 reset。
 * @retval false filter_h、name 或 config 为空，或配置含非有限值/超范围比例/超范围零偏；对象保持不变。
 * @pre 只在任务初始化阶段调用，且 filter_h 未被其他上下文使用。
 * @post 成功时 corrected_frames 为 0，配置被复制并持久保存；失败时不修改 filter_h。
 * @note 不分配动态内存；输入通道 schema 与 IMU 固定通道 3..8 一致。
 */
bool Filter_ImuCalibration_Init(
  filter_imu_calibration_t *filter_h,
  const char *name,
  const filter_imu_calibration_config_t *config
);

/**
 * @brief 在帧边界更新六轴校正参数。
 * @param filter_h 已初始化的校正滤波器，不能为 NULL。
 * @param config 非空候选参数；校验规则与 Filter_ImuCalibration_Init 相同。
 * @retval true 参数有效，已在临界区内完整提交。
 * @retval false 任一指针为空或参数无效，原配置保持不变。
 * @pre filter_h 已成功初始化；调用方拥有 config，函数只读取它。
 * @post 成功时后续 update 使用新配置；update 通过临界区复制配置，不会观察到半更新状态。
 * @note 可从参数任务调用；配置提交与帧内读取由 Sys_Critical 串行化，不分配动态内存。
 */
bool Filter_ImuCalibration_SetConfig(
  filter_imu_calibration_t *filter_h,
  const filter_imu_calibration_config_t *config
);

/**
 * @brief 复制当前六轴校正参数。
 * @param filter_h 已初始化的校正滤波器，不能为 NULL。
 * @param config 调用方拥有的输出存储，不能为 NULL。
 * @pre filter_h 已成功初始化；config 可写且不与滤波器内部配置重叠。
 * @post config 获得临界区内的完整配置快照，滤波器状态不变。
 * @note 可从其他任务读取；不分配动态内存。
 */
void Filter_ImuCalibration_GetConfig(
  const filter_imu_calibration_t *filter_h,
  filter_imu_calibration_config_t *config
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_IMU_CALIBRATION_H
