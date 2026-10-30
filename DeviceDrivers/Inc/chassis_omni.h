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
#include "policy_omni_base.h"
#include <stdint.h>

#define CHASSIS_OMNI_WHEEL_COUNT 4u

/**
 * @brief Omni 底盘对象。
 * @note motor_h[0..3] 的顺序必须与 policy_h 的安装角数组一致。
 */
typedef struct chassis_omni_s {
  chassis_base_t      base;
  motor_base_t       *motor_h[CHASSIS_OMNI_WHEEL_COUNT];
  imu_base_t         *imu_h;
  policy_omni_base_t *policy_h;

  float v;
  float dir;
  float omega;
  uint32_t imu_timeout_ms;
} chassis_omni_t;

/**
 * @brief 初始化 Omni 底盘并绑定电机、IMU、策略和生命周期操作表。
 * @param chassis_h 待初始化对象；非 NULL，生命周期和独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 chassis_base.name 保存指针而不复制。
 * @param motor_h 四轮电机基类指针数组；四个元素均非 NULL、彼此不同，数组本身不会被保存。
 * @param imu_h 已初始化的 IMU 基类指针；不拥有所有权，必须提供有效 yaw 通道，生命周期覆盖底盘。
 * @param policy_h 已初始化的 Omni 策略基类指针；不拥有所有权，轮序必须与 motor_h 一致。
 * @param imu_timeout_ms yaw 通道最大允许年龄，单位 ms；必须大于 0，超过该年龄的 yaw 会触发底盘停机。
 * @pre chassis_h、name、motor_h 中四个元素、imu_h、policy_h 非 NULL；仅调试构建会断言空指针和重复电机指针。
 * @pre 电机、IMU 和策略均已完成各自初始化，且没有控制任务并发读取该底盘对象。
 * @post chassis_base 已绑定 update/stop/estop 操作表，四个电机指针和 IMU/策略指针已保存，v/dir/omega 均为 0，imu_timeout_ms 已保存。
 * @note 不分配内存，不复制 name 或任何被指向对象；初始化本身不读取 IMU、不启动电机、不改变 GPIO/PWM 输出。
 * @warning 非 ISR 安全；只允许在设备装配阶段、控制任务启动前调用。发布构建会信任调用方满足非空和去重前置条件。
 */
void Chassis_Omni_Init(
  chassis_omni_t     *chassis_h,
  const char         *name,
  motor_base_t *const motor_h[CHASSIS_OMNI_WHEEL_COUNT],
  imu_base_t         *imu_h,
  policy_omni_base_t *policy_h,
  uint32_t            imu_timeout_ms
);
/**
 * @brief 保存 Omni 底盘的世界系平动与自转命令。
 * @param chassis_h 已初始化的 Omni 底盘对象；非 NULL。
 * @param v 世界系平动速度，单位 m/s；必须有限，本函数不施加速度幅值上限。
 * @param dir 世界系行进方向，单位 rad，逆时针为正；必须有限，不要求归一化到固定周期。
 * @param omega 自转角速度，单位 rad/s，逆时针为正；必须有限，本函数不施加角速度幅值上限。
 * @pre chassis_h 已由 Chassis_Omni_Init 初始化，且与 Update/Stop/EStop 在同一控制上下文串行执行。
 * @post 三个命令字段被逐个写入；非有限值在调试构建触发断言并中止，在定义 NDEBUG 的构建中被逐项替换为 0。
 * @note 只保存命令，不读取 IMU、不解算轮速、不改变电机目标或硬件输出；实际解算在 Chassis_Base_Update 中完成。
 * @note 不对 v/dir/omega 做归一化或限幅，超出底盘能力的命令由策略、控制律和电机 max_duty 后续约束。
 * @warning 三字段非原子更新；从 ISR 或其他任务并发调用会让 Update 观察到混合命令，必须由调用方互斥。
 */
void Chassis_Omni_SetMotion(chassis_omni_t *chassis_h, float v, float dir, float omega);

/** @} */

#endif //LAYERED_EMBEDDED_C_CHASSIS_OMNI_H
