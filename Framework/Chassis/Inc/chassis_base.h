/**
 * @file chassis_base.h
 * @brief 底盘生命周期抽象基类（100% HAL-Free）：set_motion / update / stop / estop。
 * @details 本文件属于 Framework/Chassis 层，定义面向多态底盘实现的基类与只读虚表
 *          （chassis_ops_t）。具体底盘类型须将 chassis_base_t 作为首成员并绑定静态
 *          虚表，禁止值拷贝以防对象切片；本头文件不依赖任何 HAL 或 RTOS 头文件。
 *
 * @defgroup Framework_Chassis 底盘与整车门面
 * @{
 */

#ifndef LAYERED_CAR_CHASSIS_BASE_H
#define LAYERED_CAR_CHASSIS_BASE_H

#include <stdint.h>

/** @brief 底盘多态基类前向声明（供虚表函数指针签名引用）。 */
typedef struct chassis_base_s chassis_base_t;

/**
 * @brief 车体/世界参考速度命令（Twist）。
 * @details 坐标系约定：x 轴指向车头前方，y 轴指向车体左侧，z 轴垂直向上（逆时针为正）。
 */
typedef struct chassis_twist_s {
  float vx_body_mps; /**< x 方向线速度分量（单位：m/s，车头正向，有限浮点数）。 */
  float vy_body_mps; /**< y 方向线速度分量（单位：m/s，左侧正向，有限浮点数）。 */
  float wz_radps;    /**< 绕 z 轴角速度分量（单位：rad/s，逆时针为正，有限浮点数）。 */
} chassis_twist_t;

/**
 * @brief 具体底盘操作虚表（只读函数指针集合）。
 */
typedef struct chassis_ops_s {
  void (*set_motion)(chassis_base_t *chassis_h,
                     const chassis_twist_t *twist); /**< 保存或更新期望速度命令回调（非 NULL）。 */
  void (*update)(chassis_base_t *chassis_h,
                 uint32_t now_ms); /**< 执行一次运动学解算与轮目标速度下发回调（非 NULL）。 */
  void (*stop)(chassis_base_t *chassis_h); /**< 清零速度命令并请求各轮制动停机回调（非 NULL）。 */
  void (*estop)(
      chassis_base_t *chassis_h); /**< 清零速度命令并立即禁用全部轮电机回调（非 NULL）。 */
} chassis_ops_t;

/**
 * @brief 底盘多态基类；须作为具体底盘结构体的首成员。
 */
typedef struct chassis_base_s {
  const chassis_ops_t *ops; /**< 指向具体底盘静态只读虚表的指针（只读，生命周期贯穿全程）。 */
  const char *name;         /**< 底盘实例名称字符串指针（只读，用于诊断与日志标识）。 */
} chassis_base_t;

/**
 * @brief 绑定底盘名称与静态虚表。
 * @param[out] chassis_h 待初始化的底盘基类指针（不可为 NULL）。
 * @param[in]  name      底盘名称字符串指针（不可为 NULL，须静态或长期存活）。
 * @param[in]  ops       具体底盘静态虚表指针（不可为 NULL，且 4 个函数指针均须非 NULL）。
 * @pre 仅在具体底盘 Init 阶段调用；任一参数或虚表槽位为 NULL 时静默返回。
 */
void Chassis_Base_Init(chassis_base_t *chassis_h, const char *name, const chassis_ops_t *ops);

/**
 * @brief 委托具体底盘保存目标速度命令。
 * @param[in,out] chassis_h 底盘基类句柄（不可为 NULL，且须已完成 Init）。
 * @param[in]     twist     目标速度命令指针（不可为 NULL）。
 * @note 若底盘句柄、虚表或 twist 为 NULL，则静默忽略本次调用。
 */
void Chassis_Base_SetMotion(chassis_base_t *chassis_h, const chassis_twist_t *twist);

/**
 * @brief 执行一次运动学解算与各轮目标速度下发。
 * @param[in,out] chassis_h 底盘基类句柄（不可为 NULL，且须已完成 Init）。
 * @param[in]     now_ms    当前系统单调毫秒时间戳（单位：ms，用于传感器超时判定）。
 * @note 通常由周期控制任务以固定步长调用。
 */
void Chassis_Base_Update(chassis_base_t *chassis_h, uint32_t now_ms);

/**
 * @brief 清零速度命令并请求各轮制动停机（可恢复运行）。
 * @param[in,out] chassis_h 底盘基类句柄（不可为 NULL，且须已完成 Init）。
 * @note 与 Chassis_Base_EStop() 不同，常规停机不锁存永久故障。
 */
void Chassis_Base_Stop(chassis_base_t *chassis_h);

/**
 * @brief 清零速度命令并立即禁用全部轮电机输出。
 * @param[in,out] chassis_h 底盘基类句柄（不可为 NULL，且须已完成 Init）。
 * @warning 仅在紧急停机或不可恢复故障时调用，调用后电机进入失能态。
 */
void Chassis_Base_EStop(chassis_base_t *chassis_h);

/** @} */

#endif /* LAYERED_CAR_CHASSIS_BASE_H */
