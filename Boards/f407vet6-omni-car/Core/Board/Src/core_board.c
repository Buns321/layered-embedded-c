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
#include "system_angle.h"
#include "system_log.h"
#include "uart_port.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define CAR_BOARD_WHEEL_COUNT CHASSIS_OMNI_WHEEL_COUNT

typedef struct car_board_wheel_hw_s {
  const char *encoder_name;
  const char *controller_name;
  const char *motor_name;
  platform_encoder_instance_t encoder_instance;
  platform_pwm_channel_t pwm_channel;
  platform_gpio_port_t ain1_port;
  uint16_t ain1_pin;
  platform_gpio_port_t ain2_port;
  uint16_t ain2_pin;
  platform_gpio_port_t stby_port;
  uint16_t stby_pin;
} car_board_wheel_hw_t;

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

static const car_board_wheel_hw_t s_wheel_hw[CAR_BOARD_WHEEL_COUNT] = {
    {
        .encoder_name = "encoder_a",
        .controller_name = "pid_a",
        .motor_name = "motor_a",
        .encoder_instance = PLATFORM_ENCODER_TIM2,
        .pwm_channel = PLATFORM_PWM_CHANNEL_1,
        .ain1_port = CAR_BOARD_AIN1_A_PORT,
        .ain1_pin = CAR_BOARD_AIN1_A_PIN,
        .ain2_port = CAR_BOARD_AIN2_A_PORT,
        .ain2_pin = CAR_BOARD_AIN2_A_PIN,
        .stby_port = CAR_BOARD_STBY_AB_PORT,
        .stby_pin = CAR_BOARD_STBY_AB_PIN,
    },
    {
        .encoder_name = "encoder_b",
        .controller_name = "pid_b",
        .motor_name = "motor_b",
        .encoder_instance = PLATFORM_ENCODER_TIM3,
        .pwm_channel = PLATFORM_PWM_CHANNEL_2,
        .ain1_port = CAR_BOARD_AIN1_B_PORT,
        .ain1_pin = CAR_BOARD_AIN1_B_PIN,
        .ain2_port = CAR_BOARD_AIN2_B_PORT,
        .ain2_pin = CAR_BOARD_AIN2_B_PIN,
        .stby_port = CAR_BOARD_STBY_AB_PORT,
        .stby_pin = CAR_BOARD_STBY_AB_PIN,
    },
    {
        .encoder_name = "encoder_c",
        .controller_name = "pid_c",
        .motor_name = "motor_c",
        .encoder_instance = PLATFORM_ENCODER_TIM4,
        .pwm_channel = PLATFORM_PWM_CHANNEL_3,
        .ain1_port = CAR_BOARD_AIN1_C_PORT,
        .ain1_pin = CAR_BOARD_AIN1_C_PIN,
        .ain2_port = CAR_BOARD_AIN2_C_PORT,
        .ain2_pin = CAR_BOARD_AIN2_C_PIN,
        .stby_port = CAR_BOARD_STBY_CD_PORT,
        .stby_pin = CAR_BOARD_STBY_CD_PIN,
    },
    {
        .encoder_name = "encoder_d",
        .controller_name = "pid_d",
        .motor_name = "motor_d",
        .encoder_instance = PLATFORM_ENCODER_TIM5,
        .pwm_channel = PLATFORM_PWM_CHANNEL_4,
        .ain1_port = CAR_BOARD_AIN1_D_PORT,
        .ain1_pin = CAR_BOARD_AIN1_D_PIN,
        .ain2_port = CAR_BOARD_AIN2_D_PORT,
        .ain2_pin = CAR_BOARD_AIN2_D_PIN,
        .stby_port = CAR_BOARD_STBY_CD_PORT,
        .stby_pin = CAR_BOARD_STBY_CD_PIN,
    },
};

static const float s_wheel_theta_rad[CAR_BOARD_WHEEL_COUNT] = {
    SYS_PI * 1.75f,
    SYS_PI * 1.25f,
    SYS_PI * 0.75f,
    SYS_PI * 0.25f,
};

static const float s_wheel_sign[CAR_BOARD_WHEEL_COUNT] = {
    1.0f,
    1.0f,
    1.0f,
    1.0f,
};

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
platform_status_t Core_Board_Init(app_car_dependencies_t *dependencies) {
  if (dependencies == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (s_initialized) {
    *dependencies = s_dependencies;
    return PLATFORM_STATUS_OK;
  }

  (void)memset(&s_dependencies, 0, sizeof(s_dependencies));

  if (Filter_Passthrough_Init(&s_imu_filter, "imu_passthrough") != FILTER_STATUS_OK) {
    return PLATFORM_STATUS_ERROR;
  }

  platform_status_t status = Dev_UART_Port_Init(&s_imu_uart_port, "uart1", PLATFORM_UART_1);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  Dev_IMU_WHT101_UART_Init(&s_imu, "wht101", &s_imu_uart_port.uart_base, &s_imu_filter.filter_base);
  Dev_IMU_Base_Start(&s_imu.imu_base);

  status = Dev_UART_Port_Init(&s_telemetry_uart_port, "uart2", PLATFORM_UART_2);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  Dev_JustFloat_UART_Init(&s_justfloat, "justfloat", &s_telemetry_uart_port.uart_base,
                          APP_CAR_TELEMETRY_CHANNEL_COUNT);

  status = Dev_UART_Port_Init(&s_joycon_uart_port, "uart3", PLATFORM_UART_3);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  Dev_JoyCon_UART_Init(&s_joycon, "joycon", &s_joycon_uart_port.uart_base);
  if (Dev_JoyCon_UART_Start(&s_joycon) != DEV_UART_OK) {
    return PLATFORM_STATUS_ERROR;
  }

  motor_base_t *motor_h[CAR_BOARD_WHEEL_COUNT] = {0};
  for (uint8_t wheel = 0U; wheel < CAR_BOARD_WHEEL_COUNT; wheel++) {
    const car_board_wheel_hw_t *hw = &s_wheel_hw[wheel];
    status = Dev_Encoder_TIM_Init(&s_encoders[wheel], hw->encoder_name, hw->encoder_instance);
    if (status != PLATFORM_STATUS_OK) {
      return status;
    }

    Ctrl_PID_Init(&s_pid[wheel], hw->controller_name, APP_CAR_PID_KP, APP_CAR_PID_KI,
                  APP_CAR_PID_KD, APP_CAR_MAX_WHEEL_MPS, -APP_CAR_MAX_WHEEL_MPS,
                  (float)CAR_BOARD_PWM_MAX_DUTY, -(float)CAR_BOARD_PWM_MAX_DUTY,
                  APP_CAR_PID_MAX_INTEGRAL, APP_CAR_PID_DERIVATIVE_TAU_S);

    status = Dev_Motor_TB6612_Init(
        &s_motors[wheel], hw->motor_name, &s_pid[wheel].controller_base,
        &s_encoders[wheel].encoder_base, CAR_BOARD_COUNTS_PER_METER, APP_CAR_CONTROL_PERIOD_S, 0.0f,
        CAR_BOARD_PWM_MAX_DUTY, CAR_BOARD_PWM_INSTANCE, hw->pwm_channel, hw->ain1_port,
        hw->ain1_pin, hw->ain2_port, hw->ain2_pin, hw->stby_port, hw->stby_pin);
    if (status != PLATFORM_STATUS_OK) {
      return status;
    }

    motor_h[wheel] = &s_motors[wheel].motor_base;
    s_dependencies.motor_h[wheel] = motor_h[wheel];
  }

  Policy_Omni_Kinematics_Init(&s_policy, "omni", CAR_BOARD_WHEEL_DISTANCE_M, s_wheel_theta_rad,
                              s_wheel_sign);

  Chassis_Omni_Init(&s_chassis, "chassis", motor_h, &s_imu.imu_base, &s_policy.base,
                    APP_CAR_IMU_TIMEOUT_MS);

  Dev_IMU_WHT101_UART_SetOutputRate(&s_imu, IMU_RATE_10HZ);

  s_dependencies.chassis_h = &s_chassis.base;
  s_dependencies.motor_count = CAR_BOARD_WHEEL_COUNT;
  s_dependencies.imu_h = &s_imu.imu_base;
  s_dependencies.input_h = &s_joycon.base;
  s_dependencies.telemetry_h = &s_justfloat.base;

  s_initialized = true;
  *dependencies = s_dependencies;
  SYS_LOG_INFO("BOARD", "car board initialized, motors disabled");
  return PLATFORM_STATUS_OK;
}
