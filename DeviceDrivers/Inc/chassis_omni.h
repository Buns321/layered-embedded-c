//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file chassis_omni.h
 * @brief Omni 底盘具体实现。
 * @addtogroup chassis
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CHASSIS_OMNI_H
#define LAYERED_EMBEDDED_C_CHASSIS_OMNI_H

#include "chassis_base.h"
#include "motor_base.h"
#include "imu_base.h"
#include "omni_policy_base.h"
#include <stdint.h>

#define CHASSIS_OMNI_WHEEL_COUNT 4u

/**
 * @brief Omni 底盘对象。
 * @note wheel[0..3] 的顺序必须与 policy 的安装角数组一致。
 */
typedef struct chassis_omni {
  chassis_base_t      base;
  motor_base_t       *wheel[CHASSIS_OMNI_WHEEL_COUNT];
  imu_base_t         *imu;
  omni_policy_base_t *policy;

  float v;
  float dir;
  float omega;
  uint32_t imu_timeout_ms;
} chassis_omni_t;

/**
 * @brief 初始化 Omni 底盘。
 * @param this 对象。
 * @param name 名称。
 * @param wheel 四个电机基类指针。
 * @param imu IMU 基类指针。
 * @param policy Omni 策略基类指针。
 * @param imu_timeout_ms yaw 数据最大允许年龄（ms），必须大于 0。
 * @note 底盘只保存指针，不复制对象。
 */
void Chassis_Omni_Init(
  chassis_omni_t     *this,
  const char         *name,
  motor_base_t *const wheel[CHASSIS_OMNI_WHEEL_COUNT],
  imu_base_t         *imu,
  omni_policy_base_t *policy,
  uint32_t            imu_timeout_ms
);
/**
 * @brief 设置底盘运动命令。
 * @param this 底盘对象。
 * @param v 平动速度（m/s）。
 * @param dir 世界系方向（rad，逆时针为正）。
 * @param omega 自转角速度（rad/s，逆时针为正）。
 * @note 只保存命令；实际解算在 Chassis_Base_Update 中完成。
 */
void Chassis_Omni_SetMotion(chassis_omni_t *this, float v, float dir, float omega);

/** @} */

#endif //LAYERED_EMBEDDED_C_CHASSIS_OMNI_H
