/**
 * @file chassis_omni.h
 * @brief 四轮全向底盘实现（支持 BODY 无 IMU 模式与 WORLD 无头模式）。
 * @addtogroup chassis
 * @{
 */

#ifndef LAYERED_CAR_CHASSIS_OMNI_H
#define LAYERED_CAR_CHASSIS_OMNI_H

#include "chassis_base.h"
#include "imu_base.h"
#include "kinematics_omni.h"
#include "motor_base.h"
#include <stdbool.h>
#include <stdint.h>

#define CHASSIS_OMNI_WHEEL_COUNT KINEMATICS_OMNI_WHEELS

/**
 * @brief 四轮全向底盘装配配置（CHASSIS_FRAME_BODY 时 imu 可为 NULL）。
 */
typedef struct chassis_omni_cfg_s {
  const char *name;
  motor_base_t *const *motors;
  const kinematics_omni_t *kinematics;
  imu_base_t *imu;
  chassis_frame_t frame;
  uint32_t imu_timeout_ms; /* WORLD 模式下必填 > 0，单位 ms */
} chassis_omni_cfg_t;

/**
 * @brief 四轮全向底盘静态对象（base 必须为首成员）。
 */
typedef struct chassis_omni_s {
  chassis_base_t base;
  motor_base_t *motors[CHASSIS_OMNI_WHEEL_COUNT];
  const kinematics_omni_t *kinematics;
  imu_base_t *imu;
  chassis_frame_t frame;
  chassis_twist_t command;
  uint32_t imu_timeout_ms;
} chassis_omni_t;

/**
 * @brief 校验配置并初始化全向底盘；失败返回 false 且不修改 chassis。
 */
bool Chassis_Omni_Init(chassis_omni_t *chassis, const chassis_omni_cfg_t *cfg);

/**
 * @brief 设置底盘速度命令（非有限分量自动清零）。
 */
void Chassis_Omni_SetMotion(chassis_omni_t *chassis, const chassis_twist_t *twist);

/** @} */

#endif /* LAYERED_CAR_CHASSIS_OMNI_H */
