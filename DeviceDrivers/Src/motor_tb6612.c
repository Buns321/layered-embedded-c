//
// 由 Buns_ 创建于 2026/8/24。
//

/**
 * @file motor_tb6612.c
 * @brief TB6612 电机驱动实现。
 * @ingroup drivers_concrete
 */

#include "motor_tb6612.h"
#include "system_math.h"
#include <assert.h>

/* ========= TB6612 真值表 =========
 * |------|------|-----|-----|----------|
 * | STBY | PWMA | IN1 | IN2 | 功能     |
 * |------|------|-----|-----|----------|
 * | H    | H    | H   | L   | 正转     |
 * | H    | H    | L   | H   | 反转     |
 * | H    | H    | H   | H   | 制动     |
 * | H    | H    | L   | L   | 滑行     |
 * | X    | L    | X   | X   | 滑行     |
 * | L    | X    | X   | X   | STBY     |
 * |------|------|-----|-----|----------|
 */

/*
 * 契约：
 * - power 为 [-motor_base.max_duty, +motor_base.max_duty] 的占空比，符号表示方向；
 * - 方向切换前先把 PWM 撤到 0，再更新 IN1/IN2；
 * - STBY 在板级按组共享（m0/m1、m2/m3），单电机停机不能依赖 STBY；
 * - Init 返回 platform_status_t，PWM 启动失败时保持 STBY 为低。
 */

/**
 * @brief 拉高 STBY 电平，允许 TB6612 输出级工作。
 * @param motor_h 电机的 motor_base 视图；非 NULL，且必须来自已初始化的 motor_tb6612_t 首位成员。
 * @pre motor_h 已由 Dev_Motor_TB6612_Init 初始化，STBY 引脚已由 CubeMX/Core 配置为输出。
 * @pre 仅供 Dev_Motor_Base_Enable 通过 motor_ops_tb6612 调用；同一 STBY 组不得与其他写者并发。
 * @post STBY 引脚被写为 PLATFORM_GPIO_PIN_SET；方向引脚、PWM 占空比和 motor_base.state 均不变。
 * @note GPIO 写无返回状态；本函数不检查引脚是否成功切换，也不清除之前的故障。
 * @warning STBY 可能按板级接线由多个电机共享，拉高本引脚会同时允许同组其他通道输出；非 ISR 安全。
 */
static void dev_motor_tb6612_enable(motor_base_t *motor_h) {
  assert(motor_h);
  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;
  Platform_GPIO_WritePin(motor_tb6612_h->stby_port, motor_tb6612_h->stby_pin, PLATFORM_GPIO_PIN_SET);
}

/**
 * @brief 拉低 STBY 电平，封锁 TB6612 输出级。
 * @param motor_h 电机的 motor_base 视图；非 NULL，且必须来自已初始化的 motor_tb6612_t 首位成员。
 * @pre motor_h 已由 Dev_Motor_TB6612_Init 初始化，STBY 引脚已配置为输出。
 * @pre 仅供 Dev_Motor_Base_Disable 在写零占空比之后通过 motor_ops_tb6612 调用。
 * @post STBY 引脚被写为 PLATFORM_GPIO_PIN_RESET；方向引脚、PWM 比较值、last_direction 和 motor_base.state 均不变。
 * @note GPIO 写无返回状态；本函数不主动断开发电机，只关闭驱动器输出级。
 * @warning 共享 STBY 会同时封锁同组其他电机；非 ISR 安全，不能与同组使能操作并发。
 */
static void dev_motor_tb6612_disable(motor_base_t *motor_h) {
  assert(motor_h);
  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;
  Platform_GPIO_WritePin(motor_tb6612_h->stby_port, motor_tb6612_h->stby_pin, PLATFORM_GPIO_PIN_RESET);
}

/**
 * @brief 设置 TB6612 方向与带符号占空比；零输出选择惯性滑行。
 * @param motor_h 电机的 motor_base 视图；非 NULL，且必须来自已初始化的 motor_tb6612_t 首位成员。
 * @param power 带符号占空比，单位 duty；有效范围 [-max_duty, +max_duty]，符号表示方向，0 表示 COAST。
 * @pre motor_h 已由 Dev_Motor_TB6612_Init 初始化，max_duty 大于 0，PWM 通道和方向 GPIO 已配置。
 * @pre 由 Dev_Motor_Base_Update/Disable/Reset 在同一控制上下文调用；不得与另一写者并发操作同一 PWM 或方向引脚。
 * @post 非有限 power 归零，越界值夹紧到 ±max_duty；方向改变时先写零占空比，再更新 AIN1/AIN2 和 last_direction，最后写入 |power|。
 * @post 正功率写 AIN1=SET、AIN2=RESET；负功率写 AIN1=RESET、AIN2=SET；零功率写两脚为 RESET/RESET 并进入 COAST。
 * @note Platform_PWM_SetDuty 的状态被丢弃；实例/通道错误时占空比可能保持旧值，错误只记录在 Platform_PWM_GetLastError。
 * @note 本函数不改变 STBY 或 motor_base.state；STBY 为低时方向与比较值更新不会产生实际电机输出。
 * @warning STBY 可共享，函数无法保证本通道独立上电或断电；无锁且非 ISR 安全。
 */
static void dev_motor_tb6612_setpower(motor_base_t *motor_h, float power) {
  assert(motor_h);
  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;

  float max_duty = (float)motor_tb6612_h->motor_base.max_duty;
  if (!SYS_ISFINITE(power)) power = 0.0f;
  power = SYS_CLAMP(power, -max_duty, max_duty);

  int8_t next_direction;

  if (power > 0.0f) next_direction = 1;
  else if (power < 0.0f) next_direction = -1;
  else next_direction = 0;


  if (next_direction != motor_tb6612_h->last_direction) {
    (void)Platform_PWM_SetDuty(
      motor_tb6612_h->pwm_instance,
      motor_tb6612_h->pwm_channel,
      motor_tb6612_h->motor_base.max_duty,
      0
    );

    platform_gpio_pin_state_t ain1, ain2;

    if      (next_direction > 0) { ain1 = PLATFORM_GPIO_PIN_SET;   ain2 = PLATFORM_GPIO_PIN_RESET; }
    else if (next_direction < 0) { ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_SET;   }
    else                         { ain1 = PLATFORM_GPIO_PIN_RESET; ain2 = PLATFORM_GPIO_PIN_RESET; }

    Platform_GPIO_WritePin(motor_tb6612_h->ain1_port, motor_tb6612_h->ain1_pin, ain1);
    Platform_GPIO_WritePin(motor_tb6612_h->ain2_port, motor_tb6612_h->ain2_pin, ain2);

    motor_tb6612_h->last_direction = next_direction;
  }
  uint32_t duty = (uint32_t)(power < 0.0f ? -power : power);

  (void)Platform_PWM_SetDuty(
    motor_tb6612_h->pwm_instance,
    motor_tb6612_h->pwm_channel,
    motor_tb6612_h->motor_base.max_duty,
    duty
  );
}

/**
 * @brief 将 TB6612 方向引脚置为短路制动状态并施加满占空比。
 * @param motor_h 电机的 motor_base 视图；非 NULL，且必须来自已初始化的 motor_tb6612_t 首位成员。
 * @pre motor_h 已初始化，STBY 当前为高，PWM 与方向 GPIO 可用；Dev_Motor_Base_Brake 只在非 DISABLE 状态调用本回调。
 * @pre 由电机基类在控制上下文调用，且不得与同一通道的方向或占空比写者并发。
 * @post 先写零占空比，再置 AIN1/AIN2 为 SET/SET，last_direction 置为 BRAKE，最后写满 max_duty，形成电气短路制动。
 * @post 不修改 STBY、motor_base.state、target_speed 或累计位置；返回后必须重新 Enable 才能恢复闭环输出。
 * @note 两次 Platform_PWM_SetDuty 的错误均被丢弃；PWM 写失败时制动强度可能不完整，可查询 Platform_PWM_GetLastError。
 * @warning STBY 为低时本函数不会使能制动；共享 STBY 下也不能把单电机 BRAKE 当成组级安全断电。无锁且非 ISR 安全。
 */
static void dev_motor_tb6612_brake(motor_base_t *motor_h) {
  assert(motor_h);

  motor_tb6612_t *motor_tb6612_h = (motor_tb6612_t *)motor_h;

  (void)Platform_PWM_SetDuty(
    motor_tb6612_h->pwm_instance,
    motor_tb6612_h->pwm_channel,
    motor_tb6612_h->motor_base.max_duty,
    0
  );

  Platform_GPIO_WritePin(
    motor_tb6612_h->ain1_port,
    motor_tb6612_h->ain1_pin,
    PLATFORM_GPIO_PIN_SET
  );
  Platform_GPIO_WritePin(
    motor_tb6612_h->ain2_port,
    motor_tb6612_h->ain2_pin,
    PLATFORM_GPIO_PIN_SET
  );

  motor_tb6612_h->last_direction = DEV_TB6612_DIRECTION_BRAKE;

  (void)Platform_PWM_SetDuty(
    motor_tb6612_h->pwm_instance,
    motor_tb6612_h->pwm_channel,
    motor_tb6612_h->motor_base.max_duty,
    (uint32_t)motor_tb6612_h->motor_base.max_duty
  );
}

static const motor_ops_t motor_ops_tb6612 = {
  .enable    = dev_motor_tb6612_enable,
  .disable   = dev_motor_tb6612_disable,
  .set_power = dev_motor_tb6612_setpower,
  .brake     = dev_motor_tb6612_brake,
};

/**
 * @brief 初始化 TB6612 电机、方向引脚与 PWM 通道。
 * @param motor_h 待初始化对象；非 NULL，生命周期和硬件独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 motor_base.name 保存指针而不复制。
 * @param controller_h 已初始化的轮级控制律；不拥有所有权，生命周期必须覆盖电机对象。
 * @param encoder_h 已初始化并启动的编码器；不拥有所有权，生命周期必须覆盖电机对象。
 * @param counts_per_meter 编码器每米计数，单位 counts/m；必须有限且大于 0。
 * @param control_period 控制周期，单位 s；必须有限且大于 0，必须与调用方的真实更新周期一致。
 * @param target_speed 初始目标速度，单位 m/s；必须有限，初值不触发执行器输出。
 * @param max_duty 执行器量程，单位 duty；必须大于 0，同时作为方向占空比绝对值上限。
 * @param pwm_instance PWM 定时器身份；必须启用并已完成 MX_TIMx_Init()。
 * @param pwm_channel PWM 通道身份；必须为 PLATFORM_PWM_CHANNEL_1..4，且未由其他对象独占。
 * @param ain1_port AIN1 GPIO 端口身份；必须启用并已完成 GPIO 初始化。
 * @param ain1_pin AIN1 单引脚掩码，不可为 0 或包含多个有效位。
 * @param ain2_port AIN2 GPIO 端口身份；必须启用并已完成 GPIO 初始化。
 * @param ain2_pin AIN2 单引脚掩码，不可为 0 或包含多个有效位。
 * @param stby_port STBY GPIO 端口身份；必须启用并已完成 GPIO 初始化。
 * @param stby_pin STBY 单引脚掩码，不可为 0 或包含多个有效位；可与其他电机共享。
 * @retval PLATFORM_STATUS_OK 电机基类、方向引脚和 PWM 已初始化，PWM 通道已启动；STBY 仍为低电平。
 * @retval PLATFORM_STATUS_ERROR PWM 启动失败；方向引脚和 STBY 已保持低电平，对象不可直接运行。
 * @retval PLATFORM_STATUS_BUSY PWM 定时器或通道正忙；方向引脚和 STBY 已保持低电平。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT PWM 实例或通道非法；方向引脚和 STBY 已保持低电平。
 * @pre motor_h、name、controller_h、encoder_h 非 NULL，控制律和编码器已初始化；仅 motor_h 空指针在此处调试断言，其余由 motor_base 初始化断言拦截。
 * @pre 所有 GPIO/PWM 外设已在 CubeMX/Core 中配置，初始化发生在控制任务使用电机之前，且无并发访问。
 * @post motor_base 已绑定 TB6612 操作表并保存控制器、编码器、速度换算和量程；AIN1/AIN2/STBY 被显式拉低，last_direction 为 COAST，随后尝试启动 PWM。
 * @note 不分配内存，不复制 name/controller/encoder；成功返回不会自动拉高 STBY，调用方必须通过 Dev_Motor_Base_Enable 显式使能。
 * @warning STBY 按板级接线可能共享；单电机初始化为低会同时封锁同组输出。非 ISR 安全，不得与其他控制访问并发。
 */
platform_status_t Dev_Motor_TB6612_Init(
  motor_tb6612_t        *motor_h,
  const char            *name,
  controller_base_t     *controller_h,
  encoder_base_t        *encoder_h,
  float                  counts_per_meter,
  float                  control_period,
  float                  target_speed,
  int32_t                max_duty,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel,
  platform_gpio_port_t     ain1_port, uint16_t ain1_pin,
  platform_gpio_port_t     ain2_port, uint16_t ain2_pin,
  platform_gpio_port_t     stby_port, uint16_t stby_pin
) {

  assert(motor_h);

  Dev_Motor_Base_Init(
    &motor_h->motor_base,
    name,
    &motor_ops_tb6612,
    controller_h,
    encoder_h,
    counts_per_meter,
    control_period,
    target_speed,
    max_duty
  );

  motor_h->ain1_port    = ain1_port;     motor_h->ain1_pin    = ain1_pin;
  motor_h->ain2_port    = ain2_port;     motor_h->ain2_pin    = ain2_pin;
  motor_h->stby_port    = stby_port;     motor_h->stby_pin    = stby_pin;
  motor_h->pwm_instance = pwm_instance;  motor_h->pwm_channel = pwm_channel;

  motor_h->last_direction = DEV_TB6612_DIRECTION_COAST;

  Platform_GPIO_WritePin(ain1_port, ain1_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(ain2_port, ain2_pin, PLATFORM_GPIO_PIN_RESET);
  Platform_GPIO_WritePin(stby_port, stby_pin, PLATFORM_GPIO_PIN_RESET);

  platform_status_t status = Platform_PWM_Start(pwm_instance, pwm_channel);

  if (status != PLATFORM_STATUS_OK) {
    Platform_GPIO_WritePin(
      stby_port,
      stby_pin,
      PLATFORM_GPIO_PIN_RESET
    );

    motor_h->last_direction = DEV_TB6612_DIRECTION_COAST;
    return status;
  }

  return PLATFORM_STATUS_OK;
}
