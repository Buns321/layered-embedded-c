/**
 * @file chassis_base.h
 * @brief 底盘生命周期抽象基类（100% HAL-Free）：set_motion / update / stop / estop。
 * @addtogroup chassis
 * @{
 */

#ifndef LAYERED_CAR_CHASSIS_BASE_H
#define LAYERED_CAR_CHASSIS_BASE_H

#include <stdint.h>

typedef struct chassis_base_s chassis_base_t;

/**
 * @brief 车体参考速度命令（x 车头 m/s，y 左侧 m/s，wz 逆时针绕 z 轴 rad/s）。
 */
typedef struct chassis_twist_s {
  float vx_body_mps;
  float vy_body_mps;
  float wz_radps;
} chassis_twist_t;

/**
 * @brief 具体底盘操作虚表。
 */
typedef struct chassis_ops_s {
  void (*set_motion)(chassis_base_t *chassis_h, const chassis_twist_t *twist);
  void (*update)(chassis_base_t *chassis_h, uint32_t now_ms);
  void (*stop)(chassis_base_t *chassis_h);
  void (*estop)(chassis_base_t *chassis_h);
} chassis_ops_t;

/**
 * @brief 底盘多态基类；须作为具体底盘结构体首成员。
 */
typedef struct chassis_base_s {
  const chassis_ops_t *ops;
  const char *name;
} chassis_base_t;

/** @brief 绑定底盘名称与静态虚表（仅初始化阶段调用）。 */
void Chassis_Base_Init(chassis_base_t *chassis_h, const char *name, const chassis_ops_t *ops);

/** @brief 委托具体底盘保存速度命令。 */
void Chassis_Base_SetMotion(chassis_base_t *chassis_h, const chassis_twist_t *twist);

/** @brief 执行一次运动学解算与轮目标速度下发。 */
void Chassis_Base_Update(chassis_base_t *chassis_h, uint32_t now_ms);

/** @brief 清零命令并请求各轮制动停机。 */
void Chassis_Base_Stop(chassis_base_t *chassis_h);

/** @brief 清零命令并立即禁用全部轮电机输出。 */
void Chassis_Base_EStop(chassis_base_t *chassis_h);

/** @} */

#endif /* LAYERED_CAR_CHASSIS_BASE_H */
