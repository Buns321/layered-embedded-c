/**
 * @file filter_imu_cal.h
 * @brief IMU 六轴零偏与比例校正滤波器（作用于通道 3..5 陀螺 deg/s 与 6..8 加速度 m/s^2）。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_IMU_CAL_H
#define LAYERED_CAR_FILTER_IMU_CAL_H

#include "filter_base.h"
#include <stdint.h>

/**
 * @brief IMU 六轴校正参数（scale 须在 0.1..10，|gyro_bias_dps| <= 720）。
 */
typedef struct filter_imu_cal_cfg_s {
  float accel_offset_mps2[3];
  float accel_scale[3];
  float gyro_bias_dps[3];
} filter_imu_cal_cfg_t;

/**
 * @brief IMU 六轴校正滤波器静态对象（base 必须为首成员）。
 */
typedef struct filter_imu_cal_s {
  filter_base_t base;
  filter_imu_cal_cfg_t cfg;
  uint32_t corrected_frames;
} filter_imu_cal_t;

/**
 * @brief 初始化六轴校正滤波器；失败时返回错误码且不修改对象。
 */
filter_status_t Filter_ImuCal_Init(filter_imu_cal_t *filter_h, const char *name,
                                   const filter_imu_cal_cfg_t *cfg);

/**
 * @brief 在临界区内原子更新六轴校正参数；非法参数返回错误且保持原配置。
 */
filter_status_t Filter_ImuCal_SetConfig(filter_imu_cal_t *filter_h,
                                        const filter_imu_cal_cfg_t *cfg);

/**
 * @brief 在临界区内原子读取当前六轴校正参数快照。
 */
void Filter_ImuCal_GetConfig(const filter_imu_cal_t *filter_h, filter_imu_cal_cfg_t *cfg);

/** @} */

#endif /* LAYERED_CAR_FILTER_IMU_CAL_H */
