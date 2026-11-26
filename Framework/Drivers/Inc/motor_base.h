/**
 * @file motor_base.h
 * @brief 闭环电机多态基类接口：编码器/反馈测速、SISO 控制律调度与状态机。
 * @details 属于 Framework/Drivers 设备驱动层，为具体电机执行器（如 TB6612）
 *          提供统一的速度闭环骨架、状态迁移（DISABLE/ENABLE/BRAKE）与安全限幅。
 *          所有对象均采用静态分配，禁止按值拷贝基类以避免对象切片。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_MOTOR_BASE_H
#define LAYERED_CAR_MOTOR_BASE_H

#include "bsp_encoder.h"
#include "controller_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 电机运行状态枚举。
 */
typedef enum dev_motor_state_e {
  DEV_MOTOR_DISABLE = 0U, /**< 禁用/待机态（执行器断电或高阻滑行） */
  DEV_MOTOR_ENABLE,       /**< 使能闭环态（允许周期采样并下发占空比） */
  DEV_MOTOR_BRAKE         /**< 制动态（速度目标清零并激活硬件制动） */
} dev_motor_state_t;

/** @brief 闭环电机多态基类前向声明。 */
typedef struct motor_base_s motor_base_t;

/**
 * @brief 电机执行器只读虚表（多态操作接口）。
 * @details power_duty 单位为 duty，合法范围 [-max_duty, +max_duty]。
 */
typedef struct motor_ops_s {
  void (*enable)(motor_base_t *motor_h);  /**< 使能硬件驱动器（如拉高 STBY） */
  void (*disable)(motor_base_t *motor_h); /**< 禁用硬件驱动器（如拉低 STBY） */
  void (*set_power)(
      motor_base_t *motor_h,
      float power_duty); /**< 下发有符号占空比指令（单位：duty，范围 [-max_duty, +max_duty]） */
  void (*brake)(motor_base_t *motor_h); /**< 执行硬件制动（如 H 桥短路制动） */
  float (*read_speed_mps)(
      motor_base_t *motor_h); /**< 可选速度反馈回调（单位：m/s），无外部编码器时由总线电机提供 */
} motor_ops_t;

/**
 * @brief 闭环电机多态基类结构体。
 * @note 必须作为具体电机驱动结构体的第一个成员，以便安全进行基类/派生类指针转换。
 */
typedef struct motor_base_s {
  const motor_ops_t *ops;          /**< 指向静态只读虚表的指针（非 NULL） */
  const char *name;                /**< 电机实例调试名称字符串（非 NULL） */
  controller_base_t *controller_h; /**< 绑定的 SISO 速度控制律实例句柄（非 NULL） */
  bsp_encoder_t
      *encoder_h;          /**< 绑定的正交编码器句柄（可为 NULL，此时须提供 ops->read_speed_mps） */
  dev_motor_state_t state; /**< 当前电机运行状态 */
  float target_speed_mps;  /**< 期望轮缘线速度（单位：m/s） */
  float current_speed_mps; /**< 最近周期测得的实际轮缘线速度（单位：m/s） */
  float counts_per_meter;  /**< 编码器每米行程脉冲计数（单位：counts/m，有编码器时 > 0） */
  float control_period_s;  /**< 闭环控制周期（单位：s，必须 > 0 且与 RTOS 任务周期一致） */
  uint32_t max_duty;       /**< 执行器最大占空比幅值（单位：duty，必须 > 0） */
  controller_status_t controller_status; /**< 最近一次控制律更新返回状态 */
} motor_base_t;

/**
 * @brief 绑定电机虚表、控制律与测速编码器。
 * @param[out] motor_h          电机基类指针（非 NULL）。
 * @param[in]  name             调试名称字符串（非 NULL）。
 * @param[in]  ops              只读虚表指针（enable/disable/set_power/brake 须非 NULL）。
 * @param[in]  controller_h     SISO 速度控制律句柄（非 NULL）。
 * @param[in]  encoder_h        测速编码器句柄（可为 NULL，若为 NULL 则 ops->read_speed_mps 须非
 * NULL）。
 * @param[in]  counts_per_meter 每米编码器计数（单位：counts/m；有编码器时须 > 0，无编码器时须 >=
 * 0）。
 * @param[in]  control_period_s 控制周期（单位：s，须为有限正数）。
 * @param[in]  max_duty         最大占空比限幅（单位：duty，须 > 0）。
 * @return 初始化是否成功。
 * @retval true  参数校验通过并完成基类成员初始化（初始状态为 DEV_MOTOR_DISABLE）。
 * @retval false 存在空指针、非法量纲参数或缺失必要虚表回调。
 */
bool Dev_Motor_Base_Init(motor_base_t *motor_h, const char *name, const motor_ops_t *ops,
                         controller_base_t *controller_h, bsp_encoder_t *encoder_h,
                         float counts_per_meter, float control_period_s, uint32_t max_duty);

/**
 * @brief 查询电机当前运行状态。
 * @param[in] motor_h 电机基类指针（允许为 NULL）。
 * @return 当前运行状态（motor_h 为 NULL 时返回 DEV_MOTOR_DISABLE）。
 */
dev_motor_state_t Dev_Motor_Base_ReadStatus(const motor_base_t *motor_h);

/**
 * @brief 查询最近设置的目标线速度。
 * @param[in] motor_h 电机基类指针（允许为 NULL）。
 * @return 目标线速度（单位：m/s；motor_h 为 NULL 时返回 0.0f）。
 */
float Dev_Motor_Base_ReadTargetSpeed(const motor_base_t *motor_h);

/**
 * @brief 查询最近一次闭环更新计算出的实测线速度。
 * @param[in] motor_h 电机基类指针（允许为 NULL）。
 * @return 实测线速度（单位：m/s；motor_h 为 NULL 时返回 0.0f）。
 */
float Dev_Motor_Base_ReadCurrentSpeed(const motor_base_t *motor_h);

/**
 * @brief 查询最近一次闭环更新的控制律状态。
 * @param[in] motor_h 电机基类指针（允许为 NULL）。
 * @return 控制律状态码（motor_h 为 NULL 时返回 CONTROLLER_STATUS_ERROR）。
 */
controller_status_t Dev_Motor_Base_ReadControllerStatus(const motor_base_t *motor_h);

/**
 * @brief 设置目标轮缘线速度。
 * @param[in,out] motor_h          电机基类指针（为 NULL 时安全忽略）。
 * @param[in]     target_speed_mps 目标线速度（单位：m/s；非有限值将被强制归零）。
 */
void Dev_Motor_Base_SetTargetSpeed(motor_base_t *motor_h, float target_speed_mps);

/**
 * @brief 复位闭环状态并使能执行器。
 * @details 内部先调用 Dev_Motor_Base_Reset() 归零输出并重对齐编码器基线，
 *          随后调用 ops->enable() 并将状态迁移至 DEV_MOTOR_ENABLE。
 * @param[in,out] motor_h 电机基类指针（为 NULL 或虚表无效时安全忽略）。
 */
void Dev_Motor_Base_Enable(motor_base_t *motor_h);

/**
 * @brief 先下发零占空比，再禁用执行器并清零速度目标。
 * @param[in,out] motor_h 电机基类指针（为 NULL 或虚表无效时安全忽略）。
 */
void Dev_Motor_Base_Disable(motor_base_t *motor_h);

/**
 * @brief 清零目标与当前速度并请求执行器制动。
 * @details 若当前处于 DEV_MOTOR_DISABLE，仅清零速度与状态码而不激活硬件制动。
 * @param[in,out] motor_h 电机基类指针（为 NULL 或虚表无效时安全忽略）。
 */
void Dev_Motor_Base_Brake(motor_base_t *motor_h);

/**
 * @brief 在 ENABLE 状态下采样速度、运行控制律并更新执行器占空比。
 * @param[in,out] motor_h 电机基类指针（为 NULL、非 ENABLE 状态或依赖无效时直接返回）。
 * @pre 须在固定周期 control_period_s 的控制任务中调用。
 * @warning 同一控制周期内仅允许调用一次，重复调用会导致编码器增量 delta_counts 为 0。
 */
void Dev_Motor_Base_Update(motor_base_t *motor_h);

/**
 * @brief 归零占空比输出、复位控制律并重对齐编码器测速基线。
 * @param[in,out] motor_h 电机基类指针（为 NULL 或虚表无效时安全忽略）。
 */
void Dev_Motor_Base_Reset(motor_base_t *motor_h);

#endif /* LAYERED_CAR_MOTOR_BASE_H */

/** @} */
