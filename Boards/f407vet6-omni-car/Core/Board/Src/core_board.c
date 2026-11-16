/**
 * @file core_board.c
 * @brief F407 四轮小车板级对象定义、硬件映射和依赖注入。
 * @ingroup core_board
 */

#include "core_board.h"
#include "app_car_config.h"
#include "car_board_config.h"
#include "chassis_omni.h"
#include "controller_pid.h"
#include "encoder_tim.h"
#include "filter_passthrough.h"
#include "imu_wht101_uart.h"
#include "joycon_uart.h"
#include "justfloat_uart.h"
#include "motor_tb6612.h"
#include "policy_omni_kinematics.h"
#include "system_log.h"
#include "uart_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define CAR_BOARD_WHEEL_COUNT CHASSIS_OMNI_WHEEL_COUNT
#define CAR_BOARD_PI 3.14159265358979323846f

static encoder_tim_t s_encoders[CAR_BOARD_WHEEL_COUNT];
static controller_pid_t s_pid[CAR_BOARD_WHEEL_COUNT];
static motor_tb6612_t s_motors[CAR_BOARD_WHEEL_COUNT];

static uart_port_t s_imu_uart_port;
static uart_port_t s_telemetry_uart_port;
static uart_port_t s_joycon_uart_port;
static filter_passthrough_t s_imu_filter;
static imu_wht101_uart_t s_imu;
static policy_omni_kinematics_t s_policy;
static chassis_omni_t s_chassis;
static justfloat_uart_t s_justfloat;
static joycon_uart_t s_joycon;
static app_car_dependencies_t s_dependencies;
static bool s_initialized;

static const platform_encoder_instance_t s_encoder_instances[CAR_BOARD_WHEEL_COUNT] = {
  PLATFORM_ENCODER_TIM2,
  PLATFORM_ENCODER_TIM3,
  PLATFORM_ENCODER_TIM4,
  PLATFORM_ENCODER_TIM5,
};

static const platform_pwm_channel_t s_pwm_channels[CAR_BOARD_WHEEL_COUNT] = {
  PLATFORM_PWM_CHANNEL_1,
  PLATFORM_PWM_CHANNEL_2,
  PLATFORM_PWM_CHANNEL_3,
  PLATFORM_PWM_CHANNEL_4,
};

static const float s_wheel_theta_rad[CAR_BOARD_WHEEL_COUNT] = {
  CAR_BOARD_PI * 1.75f,
  CAR_BOARD_PI * 1.25f,
  CAR_BOARD_PI * 0.75f,
  CAR_BOARD_PI * 0.25f,
};

static const float s_wheel_sign[CAR_BOARD_WHEEL_COUNT] = {
  1.0f,
  1.0f,
  1.0f,
  1.0f,
};

static const char *const s_encoder_names[CAR_BOARD_WHEEL_COUNT] = {
  "encoder_a",
  "encoder_b",
  "encoder_c",
  "encoder_d",
};

static const char *const s_controller_names[CAR_BOARD_WHEEL_COUNT] = {
  "pid_a",
  "pid_b",
  "pid_c",
  "pid_d",
};

static const char *const s_motor_names[CAR_BOARD_WHEEL_COUNT] = {
  "motor_a",
  "motor_b",
  "motor_c",
  "motor_d",
};

/**
 * @brief 返回 AIN1 GPIO 端口映射。
 * @param wheel 轮位下标，范围 0..3。
 * @return 对应端口；非法下标回退到轮位 A。
 * @note 私有纯函数。
 */
static platform_gpio_port_t car_board_ain1_port(uint8_t wheel) {
  switch (wheel) {
    case 0U: return CAR_BOARD_AIN1_A_PORT;
    case 1U: return CAR_BOARD_AIN1_B_PORT;
    case 2U: return CAR_BOARD_AIN1_C_PORT;
    case 3U: return CAR_BOARD_AIN1_D_PORT;
    default: return CAR_BOARD_AIN1_A_PORT;
  }
}

/**
 * @brief 返回 AIN1 引脚映射。
 * @param wheel 轮位下标，范围 0..3。
 * @return 对应引脚掩码；非法下标回退到轮位 A。
 * @note 私有纯函数。
 */
static uint16_t car_board_ain1_pin(uint8_t wheel) {
  switch (wheel) {
    case 0U: return CAR_BOARD_AIN1_A_PIN;
    case 1U: return CAR_BOARD_AIN1_B_PIN;
    case 2U: return CAR_BOARD_AIN1_C_PIN;
    case 3U: return CAR_BOARD_AIN1_D_PIN;
    default: return CAR_BOARD_AIN1_A_PIN;
  }
}

/**
 * @brief 返回 AIN2 GPIO 端口映射。
 * @param wheel 轮位下标，范围 0..3。
 * @return 对应端口；非法下标回退到轮位 A。
 * @note 私有纯函数。
 */
static platform_gpio_port_t car_board_ain2_port(uint8_t wheel) {
  switch (wheel) {
    case 0U: return CAR_BOARD_AIN2_A_PORT;
    case 1U: return CAR_BOARD_AIN2_B_PORT;
    case 2U: return CAR_BOARD_AIN2_C_PORT;
    case 3U: return CAR_BOARD_AIN2_D_PORT;
    default: return CAR_BOARD_AIN2_A_PORT;
  }
}

/**
 * @brief 返回 AIN2 引脚映射。
 * @param wheel 轮位下标，范围 0..3。
 * @return 对应引脚掩码；非法下标回退到轮位 A。
 * @note 私有纯函数。
 */
static uint16_t car_board_ain2_pin(uint8_t wheel) {
  switch (wheel) {
    case 0U: return CAR_BOARD_AIN2_A_PIN;
    case 1U: return CAR_BOARD_AIN2_B_PIN;
    case 2U: return CAR_BOARD_AIN2_C_PIN;
    case 3U: return CAR_BOARD_AIN2_D_PIN;
    default: return CAR_BOARD_AIN2_A_PIN;
  }
}

/**
 * @brief 返回指定轮位的共享 STBY 端口。
 * @param wheel 轮位下标，范围 0..3。
 * @return A/B 两轮返回 AB 组端口，C/D 两轮返回 CD 组端口。
 * @note 私有纯函数。
 */
static platform_gpio_port_t car_board_stby_port(uint8_t wheel) {
  return wheel < 2U
    ? CAR_BOARD_STBY_AB_PORT
    : CAR_BOARD_STBY_CD_PORT;
}

/**
 * @brief 返回指定轮位的共享 STBY 引脚。
 * @param wheel 轮位下标，范围 0..3。
 * @return A/B 两轮返回 AB 组引脚，C/D 两轮返回 CD 组引脚。
 * @note 私有纯函数。
 */
static uint16_t car_board_stby_pin(uint8_t wheel) {
  return wheel < 2U
    ? CAR_BOARD_STBY_AB_PIN
    : CAR_BOARD_STBY_CD_PIN;
}

/**
 * @brief 初始化具体设备并填写小车 App 依赖。
 * @param dependencies 调用方持有的输出依赖，不得为 NULL。
 * @retval PLATFORM_STATUS_OK 板级对象和依赖已完成初始化。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT dependencies 为 NULL。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_ERROR 设备初始化或启动失败。
 * @pre CubeMX 已初始化 TIM1、TIM2-TIM5、USART1-USART3、GPIO、DMA 和 NVIC。
 * @pre 仅在 RTOS 调度器启动前单线程调用。
 * @post 成功时具体设备保持静态生命周期，四轮保持禁用，WHT101 已请求 10 Hz 输出。
 * @note 重复调用返回已有依赖，不重新初始化硬件。
 */
platform_status_t Core_Board_Init(
  app_car_dependencies_t *dependencies
) {
  if (dependencies == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_initialized) {
    *dependencies = s_dependencies;
    return PLATFORM_STATUS_OK;
  }

  (void)memset(&s_dependencies, 0, sizeof(s_dependencies));

  if (
    Filter_Passthrough_Init(
      &s_imu_filter,
      "imu_passthrough"
    ) != FILTER_STATUS_OK
  ) {
    return PLATFORM_STATUS_ERROR;
  }

  platform_status_t status = Dev_UART_Port_Init(
    &s_imu_uart_port,
    "uart1",
    PLATFORM_UART_1
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  Dev_IMU_WHT101_UART_Init(
    &s_imu,
    "wht101",
    &s_imu_uart_port.uart_base,
    &s_imu_filter.filter_base
  );
  Dev_IMU_Base_Start(&s_imu.imu_base);

  status = Dev_UART_Port_Init(
    &s_telemetry_uart_port,
    "uart2",
    PLATFORM_UART_2
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  Dev_JustFloat_UART_Init(
    &s_justfloat,
    "justfloat",
    &s_telemetry_uart_port.uart_base,
    APP_CAR_TELEMETRY_CHANNEL_COUNT
  );

  status = Dev_UART_Port_Init(
    &s_joycon_uart_port,
    "uart3",
    PLATFORM_UART_3
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  Dev_JoyCon_UART_Init(
    &s_joycon,
    "joycon",
    &s_joycon_uart_port.uart_base
  );
  if (Dev_JoyCon_UART_Start(&s_joycon) != DEV_UART_OK) {
    return PLATFORM_STATUS_ERROR;
  }

  for (uint8_t wheel = 0U; wheel < CAR_BOARD_WHEEL_COUNT; wheel++) {
    status = Dev_Encoder_TIM_Init(
      &s_encoders[wheel],
      s_encoder_names[wheel],
      s_encoder_instances[wheel]
    );
    if (status != PLATFORM_STATUS_OK) {
      return status;
    }

    Ctrl_PID_Init(
      &s_pid[wheel],
      s_controller_names[wheel],
      APP_CAR_PID_KP,
      APP_CAR_PID_KI,
      APP_CAR_PID_KD,
      APP_CAR_MAX_WHEEL_MPS,
      -APP_CAR_MAX_WHEEL_MPS,
      (float)CAR_BOARD_PWM_MAX_DUTY,
      -(float)CAR_BOARD_PWM_MAX_DUTY,
      APP_CAR_PID_MAX_INTEGRAL,
      APP_CAR_PID_DERIVATIVE_TAU_S
    );

    status = Dev_Motor_TB6612_Init(
      &s_motors[wheel],
      s_motor_names[wheel],
      &s_pid[wheel].controller_base,
      &s_encoders[wheel].encoder_base,
      CAR_BOARD_COUNTS_PER_METER,
      APP_CAR_CONTROL_PERIOD_S,
      0.0f,
      CAR_BOARD_PWM_MAX_DUTY,
      CAR_BOARD_PWM_INSTANCE,
      s_pwm_channels[wheel],
      car_board_ain1_port(wheel),
      car_board_ain1_pin(wheel),
      car_board_ain2_port(wheel),
      car_board_ain2_pin(wheel),
      car_board_stby_port(wheel),
      car_board_stby_pin(wheel)
    );
    if (status != PLATFORM_STATUS_OK) {
      return status;
    }
  }

  Policy_Omni_Kinematics_Init(
    &s_policy,
    "omni",
    CAR_BOARD_WHEEL_DISTANCE_M,
    s_wheel_theta_rad,
    s_wheel_sign
  );

  motor_base_t *motor_h[CAR_BOARD_WHEEL_COUNT] = {
    &s_motors[0].motor_base,
    &s_motors[1].motor_base,
    &s_motors[2].motor_base,
    &s_motors[3].motor_base,
  };

  Chassis_Omni_Init(
    &s_chassis,
    "chassis",
    motor_h,
    &s_imu.imu_base,
    &s_policy.base,
    APP_CAR_IMU_TIMEOUT_MS
  );

  Dev_IMU_WHT101_UART_SetOutputRate(&s_imu, IMU_RATE_10HZ);

  s_dependencies.chassis_h = &s_chassis.base;
  for (uint8_t wheel = 0U; wheel < CAR_BOARD_WHEEL_COUNT; wheel++) {
    s_dependencies.motor_h[wheel] = &s_motors[wheel].motor_base;
  }
  s_dependencies.motor_count = CAR_BOARD_WHEEL_COUNT;
  s_dependencies.imu_h = &s_imu.imu_base;
  s_dependencies.input_h = &s_joycon.base;
  s_dependencies.telemetry_h = &s_justfloat.base;

  s_initialized = true;
  *dependencies = s_dependencies;
  SYS_LOG_INFO("BOARD", "car board initialized, motors disabled");
  return PLATFORM_STATUS_OK;
}
