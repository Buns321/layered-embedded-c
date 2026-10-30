//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file motor_base.h
 * @brief 闭环电机抽象：目标速度、编码器、控制律与执行器。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_MOTOR_BASE_H
#define LAYERED_EMBEDDED_C_MOTOR_BASE_H

#include <stdint.h>
#include "controller_base.h"

/**
 * @brief 电机使能状态。
 */
typedef enum dev_motor_state_e {
  DEV_MOTOR_DISABLE = 0U,
  DEV_MOTOR_ENABLE,
  DEV_MOTOR_BRAKE,
} dev_motor_state_t;

typedef struct motor_base_s motor_base_t;
typedef struct encoder_base_s encoder_base_t;

/**
 * @brief 电机具体实现的操作表。
 * @note set_power 的参数是带符号占空比，范围由 max_duty 约束。
 */
typedef struct motor_ops_s {
  void (*enable)    (motor_base_t *motor_h);
  void (*disable)   (motor_base_t *motor_h);
  void (*set_power) (motor_base_t *motor_h, float power); // (duty)
  void (*brake)     (motor_base_t *motor_h);
} motor_ops_t;

/**
 * @brief 闭环电机基类。
 * @note target_speed / current_speed 单位 m/s。
 * @note counts_per_meter 是编码器每米计数；control_period 单位 s。
 * @note max_duty 是执行器量程；controller_status 保存最近一次控制律状态。
 */
typedef struct motor_base_s {
  const motor_ops_t *ops;
  const char        *name;
  controller_base_t *controller_h;
  encoder_base_t    *encoder_h;

  dev_motor_state_t    state;            // 禁用 / 闭环使能 / 制动保持

  float              target_speed;     // (m/s)
  float              current_speed;    // (m/s)
  float              counts_per_meter; // (counts/m)
  float              control_period;   // (s)
  int32_t            max_duty;         // (duty)
  controller_status_t controller_status;
} motor_base_t;

/**
 * @brief 绑定电机输出、控制律、编码器与标定参数。
 * @param motor_h 调用方持有的电机基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖电机对象；不可为 NULL。
 * @param ops 静态输出操作表；不可为 NULL，具体操作在首次使用时校验。
 * @param controller_h 已初始化的控制器句柄；非拥有，生命周期必须覆盖电机。
 * @param encoder_h 已初始化的编码器句柄；非拥有，生命周期必须覆盖电机。
 * @param counts_per_meter 编码器每米计数，单位 counts/m；必须有限且大于 0。
 * @param control_period 控制周期，单位 s；必须有限、大于 0，并与实际调用周期一致。
 * @param target_speed 初始目标速度，单位 m/s；必须有限，正负号表示方向。
 * @param max_duty 执行器正向量程，单位 duty；必须是大于 0 的整数。
 * @pre motor_h 尚未并发发布，所有非拥有指针在电机寿命内保持有效。
 * @post 保存绑定与参数，状态置为 DEV_MOTOR_DISABLE，当前速度清零，目标经目标设置接口写入。
 * @note 仅限初始化阶段调用；不访问硬件、不加锁、不分配内存。
 */
void Dev_Motor_Base_Init(
  motor_base_t      *motor_h,
  const char        *name,
  const motor_ops_t *ops,
  controller_base_t *controller_h,
  encoder_base_t    *encoder_h,
  float              counts_per_meter,
  float              control_period,
  float              target_speed,
  int32_t            max_duty
);
/**
 * @brief 读取电机运行状态。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @retval DEV_MOTOR_DISABLE 输出禁用且目标清零。
 * @retval DEV_MOTOR_ENABLE 闭环更新允许输出。
 * @retval DEV_MOTOR_BRAKE 制动保持，Update 不再输出。
 * @pre motor_h 已通过 Dev_Motor_Base_Init 初始化。
 * @post 不读取编码器、不改变状态或执行器输出。
 * @note 本层不加锁；与控制任务并发读取时可能观察到瞬时旧值。
 */
dev_motor_state_t Dev_Motor_Base_ReadStatus       (motor_base_t *motor_h);
/**
 * @brief 读取最近一次保存的目标速度。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @return 有符号目标速度，单位 m/s；范围为 float 可表示的有限值。
 * @pre motor_h 已初始化；目标值不因读取而改变。
 * @post 不读取编码器、不运行控制律，也不修改执行器。
 * @note 该值可能尚未被 Update 消费，不能据此推断当前速度。
 */
float           Dev_Motor_Base_ReadTargetSpeed  (motor_base_t *motor_h);
/**
 * @brief 读取最近一次闭环更新计算出的电机速度。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @return 有符号速度，单位 m/s；初始化、复位、制动或换算失败后为 0。
 * @pre motor_h 已初始化；最近一次 Update 未发生可报告的硬件异常。
 * @post 不读取编码器、不改变控制状态或执行器输出。
 * @note 该值可能相对真实转速滞后一个控制周期，且不提供数据新鲜度。
 */
float           Dev_Motor_Base_ReadCurrentSpeed (motor_base_t *motor_h);
/**
 * @brief 读取最近一次闭环更新的控制律状态。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @retval CONTROLLER_STATUS_OK 最近一次输出有效且未触及控制或执行器限幅。
 * @retval CONTROLLER_STATUS_SATURATED 最近一次输出被控制律或 max_duty 限幅。
 * @retval CONTROLLER_STATUS_ERROR 换算、控制输出或非有限值检查失败，执行器已收到零输出。
 * @pre motor_h 已初始化；状态在首次 Update 前为 OK。
 * @post 不重新计算控制律，不改变任何状态。
 * @note 状态只反映最近一次 Update，过早读取可能看到旧结果。
 */
controller_status_t Dev_Motor_Base_ReadControllerStatus(motor_base_t *motor_h);
/**
 * @brief 保存新的目标速度而不立即产生输出。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @param target_speed 有符号目标速度，单位 m/s；必须有限，不在本函数内限幅。
 * @pre motor_h 已初始化；调用方负责在下一控制周期运行 Update。
 * @post 目标值被替换；非有限输入在关闭 assert 的构建中按 0 保存。
 * @note 不改变使能状态、当前速度、控制律状态或执行器输出。
 * @warning 本层不加锁；与 Update 并发写入时存在数据竞争。
 */
void            Dev_Motor_Base_SetTargetSpeed   (motor_base_t *motor_h, float target_speed);
/**
 * @brief 复位闭环状态后请求具体驱动器使能。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @pre enable、set_power 操作可用，控制器和编码器已初始化且可复位。
 * @post 先应用零输出、复位控制律并重同步编码器，再调用使能操作并把状态置为 ENABLE。
 * @note 目标速度保留；通常在切换运行状态的任务上下文调用，本层不加锁。
 * @warning 具体使能失败无错误码；状态仍记为 ENABLE，需检查硬件反馈。
 */
void            Dev_Motor_Base_Enable           (motor_base_t *motor_h);
/**
 * @brief 先下发零输出，再禁用电机并清除目标。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @pre set_power 与 disable 操作可用。
 * @post 状态置为 DISABLE，目标速度和当前速度清零，控制律状态置为 OK。
 * @note 不额外复位控制律或重新对齐编码器基线；本层不加锁。
 * @warning 具体输出或禁用失败无错误码，调用方应通过硬件状态确认停机。
 */
void            Dev_Motor_Base_Disable          (motor_base_t *motor_h);
/**
 * @brief 清零速度量并请求具体驱动器制动。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @pre brake 操作与控制器可用；调用前已完成初始化。
 * @post 目标与当前速度清零、控制状态置为 OK；原状态为 DISABLE 时保持禁用，否则请求制动并置为 BRAKE。
 * @note 制动后 Update 不再输出，恢复闭环必须显式调用 Enable。
 * @warning 本层不加锁；并发 Update、Enable 或 Disable 可能覆盖状态转换。
 */
void            Dev_Motor_Base_Brake            (motor_base_t *motor_h);
/**
 * @brief 执行一次编码器采样、速度换算、闭环计算与占空比下发。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @pre 电机已初始化；仅 ENABLE 状态执行采样与输出，set_power、控制器和编码器可用。
 * @post ENABLE 状态下消费一次编码器增量，更新 current_speed、controller_status，并向执行器写入限幅到 ±max_duty 的 duty。
 * @warning 非 ENABLE 状态直接返回且不读取编码器；每个控制周期只能对同一电机的使能状态调用一次。
 * @note 换算或控制输出非法时清零输出并记录 ERROR；饱和时记录 SATURATED。本层不加锁。
 */
void            Dev_Motor_Base_Update           (motor_base_t *motor_h);
/**
 * @brief 清零执行器输出、复位控制律并重新对齐编码器基线。
 * @param motor_h 已初始化的电机句柄；不可为 NULL，所有权不转移。
 * @pre set_power、控制器和编码器可用。
 * @post 输出为零，控制律状态为 OK，当前速度清零，编码器以当前硬件计数建立新增量基线。
 * @note 不改变目标速度和 DEV_MOTOR_ENABLE/BRAKE/DISABLE 状态；通常在使能或故障恢复时调用。
 * @warning 本层不加锁；并发 Update 可能在下发零输出后再次写入执行器。
 */
void            Dev_Motor_Base_Reset            (motor_base_t *motor_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_MOTOR_BASE_H
