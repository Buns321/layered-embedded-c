/**
 * @file chassis_omni.h
 * @brief 四轮全向底盘实现（支持 BODY 无 IMU 模式与 WORLD 无头模式）。
 * @details 本文件属于 Framework/Chassis 层，继承 chassis_base_t 多态基类，
 *          将车体/世界坐标系速度命令经 kinematics_omni 逆解为四轮目标线速度并下发给
 *          motor_base_t 执行器。所有成员均为静态装配，无动态内存分配。
 *
 * @addtogroup Framework_Chassis
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

/** @brief 四轮全向底盘轮数常量（与运动学模块保持一致，固定为 4）。 */
#define CHASSIS_OMNI_WHEEL_COUNT KINEMATICS_OMNI_WHEELS

/**
 * @brief 四轮全向底盘装配配置。
 * @note 当 frame 为 CHASSIS_FRAME_BODY 时 imu 可为 NULL；当 frame 为 CHASSIS_FRAME_WORLD
 *       时 imu 不可为 NULL 且 imu_timeout_ms 必须大于 0。
 */
typedef struct chassis_omni_cfg_s {
  const char *name;            /**< 底盘实例名称字符串指针（不可为 NULL）。 */
  motor_base_t *const *motors; /**< 长度为 4 的轮电机句柄数组指针（元素不可为 NULL 或重复）。 */
  const kinematics_omni_t *kinematics; /**< 已初始化的四轮全向运动学模型只读指针（不可为 NULL）。 */
  imu_base_t *imu;         /**< IMU 多态基类句柄（WORLD 模式必填，BODY 模式可为 NULL）。 */
  chassis_frame_t frame;   /**< 控制参考坐标系（CHASSIS_FRAME_BODY 或 CHASSIS_FRAME_WORLD）。 */
  uint32_t imu_timeout_ms; /**< WORLD 模式下 IMU 偏航角有效超时阈值（单位：ms，须 > 0）。 */
} chassis_omni_cfg_t;

/**
 * @brief 四轮全向底盘静态对象（base 必须为首成员）。
 */
typedef struct chassis_omni_s {
  chassis_base_t base; /**< 多态基类首成员（供 Chassis_Base_* 强转）。 */
  motor_base_t
      *motors[CHASSIS_OMNI_WHEEL_COUNT]; /**< 四路轮电机句柄缓存（按轮位下标 0..3 排列）。 */
  const kinematics_omni_t *kinematics;   /**< 全向运动学逆解参数只读指针。 */
  imu_base_t *imu;                       /**< 可选 IMU 基类句柄（WORLD 模式用于读取 yaw_deg）。 */
  chassis_frame_t frame;                 /**< 当前工作坐标系模式（BODY 或 WORLD）。 */
  chassis_twist_t command; /**< 最近一次下发并经有限值清洗后的期望速度（m/s, rad/s）。 */
  uint32_t imu_timeout_ms; /**< WORLD 模式下 IMU 偏航角新鲜度超时（单位：ms）。 */
} chassis_omni_t;

/**
 * @brief 校验配置并初始化全向底盘。
 * @param[out] chassis 待初始化的四轮全向底盘静态对象指针（不可为 NULL）。
 * @param[in]  cfg     底盘装配配置指针（不可为 NULL）。
 * @return 初始化成功与否。
 * @retval true  全部参数、轮电机指针（非 NULL 且互不重复）及模式约束校验通过并完成初始化。
 * @retval false 参数非法（此时不修改 chassis 内容）。
 * @pre motors 指向的 4 个电机与 kinematics 须已完成各自 Init。
 */
bool Chassis_Omni_Init(chassis_omni_t *chassis, const chassis_omni_cfg_t *cfg);

/**
 * @brief 设置底盘期望速度命令（非有限分量自动清零）。
 * @param[in,out] chassis 已初始化的四轮全向底盘指针（不可为 NULL）。
 * @param[in]     twist   目标速度命令指针（不可为 NULL）。
 * @note 若 chassis 为 NULL 则静默返回；NaN/Inf 分量会被清洗为 0.0f。
 */
void Chassis_Omni_SetMotion(chassis_omni_t *chassis, const chassis_twist_t *twist);

/** @} */

#endif /* LAYERED_CAR_CHASSIS_OMNI_H */
