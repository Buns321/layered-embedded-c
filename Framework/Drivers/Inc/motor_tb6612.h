/**
 * @file motor_tb6612.h
 * @brief TB6612 双极性直流电机驱动接口。
 * @details 属于 Framework/Drivers 设备驱动层，基于 motor_base_t 多态基类与
 *          BSP_PWM/BSP_GPIO 实现单配置结构体初始化、换向先撤占空比保护、
 *          H 桥短路制动与紧急断电停机。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_MOTOR_TB6612_H
#define LAYERED_CAR_MOTOR_TB6612_H

#include "bsp_def.h"
#include "bsp_gpio.h"
#include "bsp_pwm.h"
#include "motor_base.h"

/**
 * @brief TB6612 H 桥当前逻辑方向状态。
 */
typedef enum motor_tb6612_dir_e {
  DEV_TB6612_DIR_REVERSE = -1, /**< 反转（IN1=0, IN2=1） */
  DEV_TB6612_DIR_COAST = 0,    /**< 滑行/停止（IN1=0, IN2=0） */
  DEV_TB6612_DIR_FORWARD = 1,  /**< 正转（IN1=1, IN2=0） */
  DEV_TB6612_DIR_BRAKE = 2     /**< 短路制动（IN1=1, IN2=1, PWM=max_duty） */
} motor_tb6612_dir_t;

/**
 * @brief TB6612 电机初始化配置结构体。
 * @details stby 允许共享给多路电机，或置为 {NULL, 0} 表示由外部硬件常使能。
 */
typedef struct motor_tb6612_cfg_s {
  const char *name;              /**< 电机调试名称字符串（非 NULL） */
  bsp_pwm_t pwm;                 /**< PWM 输出通道句柄（max_duty=0 时自动取 ARR+1） */
  bsp_gpio_t in1;                /**< H 桥方向控制引脚 IN1（必须有效） */
  bsp_gpio_t in2;                /**< H 桥方向控制引脚 IN2（必须有效） */
  bsp_gpio_t stby;               /**< 待机使能引脚 STBY（允许 {NULL, 0}） */
  bsp_encoder_t *encoder;        /**< 绑定的编码器句柄（非 NULL） */
  controller_base_t *controller; /**< 绑定的速度控制律句柄（非 NULL） */
  float counts_per_meter;        /**< 编码器每米行程计数（单位：counts/m，须 > 0） */
  float control_period_s;        /**< 闭环控制周期（单位：s，须 > 0） */
} motor_tb6612_cfg_t;

/**
 * @brief TB6612 电机静态对象结构体。
 * @note base 必须作为首成员以支持 motor_base_t* 多态指针转换。
 */
typedef struct motor_tb6612_s {
  motor_base_t base;                 /**< 闭环电机多态基类（必须为首成员） */
  motor_tb6612_dir_t last_direction; /**< 上一次生效的 H 桥逻辑方向，用于换向去抖与先撤占空比 */
  bsp_pwm_t pwm;                     /**< 解析有效 max_duty 后的 PWM 通道句柄 */
  bsp_gpio_t in1;                    /**< H 桥方向引脚 IN1 */
  bsp_gpio_t in2;                    /**< H 桥方向引脚 IN2 */
  bsp_gpio_t stby;                   /**< 待机引脚 STBY */
} motor_tb6612_t;

/**
 * @brief 初始化 TB6612 电机并启动 PWM 通道。
 * @details 初始化后 IN1/IN2/STBY 均保持低电平、PWM 占空比为 0，
 *          须显式调用 Dev_Motor_Base_Enable(&motor->base) 才可输出动力。
 * @param[out] motor TB6612 静态对象指针（非 NULL）。
 * @param[in]  cfg   初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              参数合法、基类绑定成功且 PWM 通道已启动。
 * @retval BSP_ERR_INVALID_ARG 参数为空、引脚/PWM/量纲非法或 max_duty 解析失败。
 * @retval 其他 bsp_status_t   底层 BSP_PWM_Start 返回的错误码。
 * @pre 对应 TIM、GPIO 及编码器定时器外设已完成 CubeMX 初始化。
 */
bsp_status_t Dev_Motor_TB6612_Init(motor_tb6612_t *motor, const motor_tb6612_cfg_t *cfg);

/**
 * @brief 紧急断电停机。
 * @details 直接将 PWM 占空比清零并拉低 IN1/IN2/STBY，同时复位基类运行状态为
 *          DEV_MOTOR_DISABLE。适用于 HardFault、栈溢出或看门狗超时安全钩子。
 * @param[in,out] motor TB6612 对象指针（为 NULL 时安全忽略）。
 * @note 可在异常钩子或中断上下文中安全调用。
 */
void Dev_Motor_TB6612_EmergencyHalt(motor_tb6612_t *motor);

#endif /* LAYERED_CAR_MOTOR_TB6612_H */

/** @} */
