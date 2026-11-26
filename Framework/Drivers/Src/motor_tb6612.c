/**
 * @file motor_tb6612.c
 * @brief TB6612 双极性直流电机驱动实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现 TB6612 H 桥的换向先撤占空比
 *          无冲击切换、短路制动时序（IN1=IN2=HIGH, PWM=max_duty）及紧急断电停机。
 * @ingroup Framework_Drivers
 */

#include "motor_tb6612.h"
#include "sys_math.h"
#include <stddef.h>

/**
 * @brief 校验 PWM 句柄及通道枚举是否有效。
 * @param[in] pwm PWM 句柄指针。
 * @return 句柄是否有效。
 * @retval true  htim 非空且 channel 为 TIM_CHANNEL_1..4 之一。
 * @retval false 句柄为空或通道非法。
 */
static bool tb6612_pwm_isvalid(const bsp_pwm_t *pwm) {
  if (pwm == NULL || pwm->htim == NULL) {
    return false;
  }
  return pwm->channel == TIM_CHANNEL_1 || pwm->channel == TIM_CHANNEL_2 ||
         pwm->channel == TIM_CHANNEL_3 || pwm->channel == TIM_CHANNEL_4;
}

/**
 * @brief TB6612 虚表回调：拉高 STBY 引脚使能 H 桥输出。
 * @param[in,out] motor_h 电机基类指针（实际指向 motor_tb6612_t）。
 */
static void tb6612_enable(motor_base_t *motor_h) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor != NULL && BSP_GPIO_IsValid(motor->stby)) {
    BSP_GPIO_Write(motor->stby, true);
  }
}

/**
 * @brief TB6612 虚表回调：拉低 STBY 引脚使 H 桥进入高阻待机态。
 * @param[in,out] motor_h 电机基类指针（实际指向 motor_tb6612_t）。
 */
static void tb6612_disable(motor_base_t *motor_h) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor != NULL && BSP_GPIO_IsValid(motor->stby)) {
    BSP_GPIO_Write(motor->stby, false);
  }
}

/**
 * @brief TB6612 虚表回调：设置带符号占空比并安全切换 H 桥方向。
 * @param[in,out] motor_h    电机基类指针（实际指向 motor_tb6612_t）。
 * @param[in]     power_duty 有符号占空比指令（单位：duty，范围 [-max_duty, +max_duty]）。
 */
static void tb6612_setpower(motor_base_t *motor_h, float power_duty) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor == NULL) {
    return;
  }

  float max_duty = (float)motor->base.max_duty;
  if (!SYS_ISFINITE(power_duty)) {
    power_duty = 0.0f;
  }
  power_duty = SYS_CLAMP(power_duty, -max_duty, max_duty);

  motor_tb6612_dir_t next_dir = DEV_TB6612_DIR_COAST;
  if (power_duty > 0.0f) {
    next_dir = DEV_TB6612_DIR_FORWARD;
  } else if (power_duty < 0.0f) {
    next_dir = DEV_TB6612_DIR_REVERSE;
  }

  if (next_dir != motor->last_direction) {
    // 换向死区保护时序：先将 PWM 占空比置 0 关断上下管驱动，再翻转 IN1/IN2 逻辑电平，
    // 防止在旧占空比仍有效期间出现桥臂瞬间反接或电源电流尖峰冲击
    (void)BSP_PWM_SetDuty(&motor->pwm, 0U);
    if (next_dir == DEV_TB6612_DIR_FORWARD) {
      BSP_GPIO_Write(motor->in1, true);
      BSP_GPIO_Write(motor->in2, false);
    } else if (next_dir == DEV_TB6612_DIR_REVERSE) {
      BSP_GPIO_Write(motor->in1, false);
      BSP_GPIO_Write(motor->in2, true);
    } else {
      BSP_GPIO_Write(motor->in1, false);
      BSP_GPIO_Write(motor->in2, false);
    }
    motor->last_direction = next_dir;
  }

  // 方向确定后再写入绝对占空比幅值，确保新方向在零电流/零占空比下平滑建立
  uint32_t duty = (uint32_t)SYS_ABS(power_duty);
  (void)BSP_PWM_SetDuty(&motor->pwm, duty);
}

/**
 * @brief TB6612 虚表回调：执行 H 桥短路制动。
 * @param[in,out] motor_h 电机基类指针（实际指向 motor_tb6612_t）。
 */
static void tb6612_brake(motor_base_t *motor_h) {
  motor_tb6612_t *motor = (motor_tb6612_t *)motor_h;
  if (motor == NULL) {
    return;
  }

  // 短路制动时序：先撤占空比至 0 消除驱动转矩，再置 IN1=IN2=HIGH 使下桥臂短接电机两端，
  // 最后将 PWM 拉满（max_duty）以维持全周期低阻续流制动
  (void)BSP_PWM_SetDuty(&motor->pwm, 0U);
  BSP_GPIO_Write(motor->in1, true);
  BSP_GPIO_Write(motor->in2, true);
  motor->last_direction = DEV_TB6612_DIR_BRAKE;
  (void)BSP_PWM_SetDuty(&motor->pwm, motor->pwm.max_duty);
}

/** @brief TB6612 电机多态虚表实例。 */
static const motor_ops_t s_motor_ops_tb6612 = {
    .enable = tb6612_enable,
    .disable = tb6612_disable,
    .set_power = tb6612_setpower,
    .brake = tb6612_brake,
    .read_speed_mps = NULL,
};

bsp_status_t Dev_Motor_TB6612_Init(motor_tb6612_t *motor, const motor_tb6612_cfg_t *cfg) {
  if (motor == NULL || cfg == NULL || cfg->name == NULL || !tb6612_pwm_isvalid(&cfg->pwm) ||
      !BSP_GPIO_IsValid(cfg->in1) || !BSP_GPIO_IsValid(cfg->in2) || cfg->encoder == NULL ||
      cfg->controller == NULL || !SYS_IS_POS_FINITE(cfg->counts_per_meter) ||
      !SYS_IS_POS_FINITE(cfg->control_period_s)) {
    return BSP_ERR_INVALID_ARG;
  }

  // stby 全零表示硬件常使能；若配置了任意非零字段则必须通过完整 GPIO 有效性校验
  bool has_stby = (cfg->stby.port != NULL || cfg->stby.pin != 0U);
  if (has_stby && !BSP_GPIO_IsValid(cfg->stby)) {
    return BSP_ERR_INVALID_ARG;
  }

  // max_duty 为 0 时自动回退为定时器 ARR + 1，对齐 BSP_PWM 满量程定义
  uint32_t effective_max_duty =
      (cfg->pwm.max_duty > 0U) ? cfg->pwm.max_duty : (__HAL_TIM_GET_AUTORELOAD(cfg->pwm.htim) + 1U);
  if (effective_max_duty == 0U) {
    return BSP_ERR_INVALID_ARG;
  }

  bsp_pwm_t resolved_pwm = cfg->pwm;
  resolved_pwm.max_duty = effective_max_duty;

  if (!Dev_Motor_Base_Init(&motor->base, cfg->name, &s_motor_ops_tb6612, cfg->controller,
                           cfg->encoder, cfg->counts_per_meter, cfg->control_period_s,
                           effective_max_duty)) {
    return BSP_ERR_INVALID_ARG;
  }

  motor->pwm = resolved_pwm;
  motor->in1 = cfg->in1;
  motor->in2 = cfg->in2;
  motor->stby = cfg->stby;
  motor->last_direction = DEV_TB6612_DIR_COAST;

  // 上电初始保持滑行且 STBY 拉低，等待上层显式 Enable
  BSP_GPIO_Write(motor->in1, false);
  BSP_GPIO_Write(motor->in2, false);
  if (has_stby) {
    BSP_GPIO_Write(motor->stby, false);
  }
  (void)BSP_PWM_SetDuty(&motor->pwm, 0U);

  bsp_status_t st = BSP_PWM_Start(&motor->pwm);
  if (st != BSP_OK) {
    if (has_stby) {
      BSP_GPIO_Write(motor->stby, false);
    }
    return st;
  }

  return BSP_OK;
}

void Dev_Motor_TB6612_EmergencyHalt(motor_tb6612_t *motor) {
  if (motor == NULL) {
    return;
  }
  if (tb6612_pwm_isvalid(&motor->pwm)) {
    (void)BSP_PWM_SetDuty(&motor->pwm, 0U);
  }
  BSP_GPIO_Write(motor->in1, false);
  BSP_GPIO_Write(motor->in2, false);
  if (BSP_GPIO_IsValid(motor->stby)) {
    BSP_GPIO_Write(motor->stby, false);
  }
  motor->last_direction = DEV_TB6612_DIR_COAST;
  motor->base.state = DEV_MOTOR_DISABLE;
  motor->base.target_speed_mps = 0.0f;
  motor->base.current_speed_mps = 0.0f;
}
