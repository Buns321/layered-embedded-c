//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file app_car.c
 * @brief F407 四轮全向小车设备装配、控制与监控实现。
 * @ingroup app
 */

#include "app_car.h"
#include "app_car_config.h"
#include "chassis_omni.h"
#include "controller_pid.h"
#include "encoder_tim.h"
#include "filter_passthrough.h"
#include "imu_wht101_uart.h"
#include "joycon_uart.h"
#include "justfloat_uart.h"
#include "motor_tb6612.h"
#include "policy_omni_kinematics.h"
#include "uart_port.h"
#include "system_critical.h"
#include "system_log.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define APP_CAR_WHEEL_COUNT 4u
#define APP_CAR_TELEMETRY_CHANNEL_COUNT 16u
#define APP_CAR_PI 3.14159265358979323846f

typedef enum app_car_boot_state_e {
  APP_CAR_BOOT_RATE_CONFIG = 0U,
  APP_CAR_BOOT_WAIT_STILL,
  APP_CAR_BOOT_ZERO_CONFIG,
  APP_CAR_BOOT_WAIT_ZERO_FRAME,
  APP_CAR_BOOT_READY,
  APP_CAR_BOOT_FAULT,
} app_car_boot_state_t;

typedef enum app_car_telemetry_channel_e {
  APP_CAR_TELEMETRY_TARGET_A = 0U,
  APP_CAR_TELEMETRY_TARGET_B,
  APP_CAR_TELEMETRY_TARGET_C,
  APP_CAR_TELEMETRY_TARGET_D,
  APP_CAR_TELEMETRY_CURRENT_A,
  APP_CAR_TELEMETRY_CURRENT_B,
  APP_CAR_TELEMETRY_CURRENT_C,
  APP_CAR_TELEMETRY_CURRENT_D,
  APP_CAR_TELEMETRY_LINEAR_SPEED,
  APP_CAR_TELEMETRY_DIRECTION,
  APP_CAR_TELEMETRY_ANGULAR_SPEED,
  APP_CAR_TELEMETRY_YAW,
  APP_CAR_TELEMETRY_GYRO_Z,
  APP_CAR_TELEMETRY_JOYCON_LEFT_X,
  APP_CAR_TELEMETRY_JOYCON_LEFT_Y,
  APP_CAR_TELEMETRY_JOYCON_RIGHT_X,
} app_car_telemetry_channel_t;

typedef struct app_car_command_s {
  float linear_speed;
  float direction;
  float angular_speed;
  float joycon_left_x;
  float joycon_left_y;
  float joycon_right_x;
  uint32_t time_ms;
  bool valid;
} app_car_command_t;

typedef struct app_car_state_s {
  volatile app_car_boot_state_t boot_state;
  uint32_t state_started_ms;
  uint32_t config_done_ms;
  uint32_t still_since_ms;
  bool still_tracking;
  uint32_t zero_done_ms;
  uint32_t last_telemetry_ms;
  uint32_t last_log_ms;
  volatile uint32_t control_heartbeat_ms;
  volatile bool control_heartbeat_valid;
  volatile bool imu_ready;
  volatile bool faulted;
  volatile bool estop_latched;
  volatile bool motors_enabled;
  app_car_command_t command;
  bool initialized;
} app_car_state_t;

static encoder_tim_t s_encoders[APP_CAR_WHEEL_COUNT];
static controller_pid_t s_pid[APP_CAR_WHEEL_COUNT];
static motor_tb6612_t s_motors[APP_CAR_WHEEL_COUNT];

static uart_port_t s_imu_uart_port;
static uart_port_t s_telemetry_uart_port;
static uart_port_t s_joycon_uart_port;
static filter_passthrough_t s_imu_filter;
static imu_wht101_uart_t s_imu;
static policy_omni_kinematics_t s_policy;
static chassis_omni_t s_chassis;
static justfloat_uart_t s_justfloat;
static joycon_uart_t s_joycon;
static app_car_state_t s_car;

static const platform_encoder_instance_t s_encoder_instances[APP_CAR_WHEEL_COUNT] = {
  PLATFORM_ENCODER_TIM2,
  PLATFORM_ENCODER_TIM3,
  PLATFORM_ENCODER_TIM4,
  PLATFORM_ENCODER_TIM5,
};

static const platform_pwm_channel_t s_pwm_channels[APP_CAR_WHEEL_COUNT] = {
  PLATFORM_PWM_CHANNEL_1,
  PLATFORM_PWM_CHANNEL_2,
  PLATFORM_PWM_CHANNEL_3,
  PLATFORM_PWM_CHANNEL_4,
};

static const float s_wheel_theta[APP_CAR_WHEEL_COUNT] = {
  APP_CAR_PI * 1.75f,  // A：1:30，前右，315° / -45°
  APP_CAR_PI * 1.25f,  // B：4:30，后右，225° / -135°
  APP_CAR_PI * 0.75f,  // C：7:30，后左，135°
  APP_CAR_PI * 0.25f,  // D：10:30，前左，45°
};

static const float s_wheel_sign[APP_CAR_WHEEL_COUNT] = {
  1.0f,
  1.0f,
  1.0f,
  1.0f,
};

static const char *const s_encoder_names[APP_CAR_WHEEL_COUNT] = {
  "encoder_a",
  "encoder_b",
  "encoder_c",
  "encoder_d",
};

static const char *const s_controller_names[APP_CAR_WHEEL_COUNT] = {
  "pid_a",
  "pid_b",
  "pid_c",
  "pid_d",
};

static const char *const s_motor_names[APP_CAR_WHEEL_COUNT] = {
  "motor_a",
  "motor_b",
  "motor_c",
  "motor_d",
};

static platform_gpio_port_t app_car_ain1_port(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN1_A_PORT;
    case 1U: return APP_CAR_AIN1_B_PORT;
    case 2U: return APP_CAR_AIN1_C_PORT;
    case 3U: return APP_CAR_AIN1_D_PORT;
    default: return APP_CAR_AIN1_A_PORT;
  }
}

static uint16_t app_car_ain1_pin(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN1_A_PIN;
    case 1U: return APP_CAR_AIN1_B_PIN;
    case 2U: return APP_CAR_AIN1_C_PIN;
    case 3U: return APP_CAR_AIN1_D_PIN;
    default: return APP_CAR_AIN1_A_PIN;
  }
}

static platform_gpio_port_t app_car_ain2_port(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN2_A_PORT;
    case 1U: return APP_CAR_AIN2_B_PORT;
    case 2U: return APP_CAR_AIN2_C_PORT;
    case 3U: return APP_CAR_AIN2_D_PORT;
    default: return APP_CAR_AIN2_A_PORT;
  }
}

static uint16_t app_car_ain2_pin(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN2_A_PIN;
    case 1U: return APP_CAR_AIN2_B_PIN;
    case 2U: return APP_CAR_AIN2_C_PIN;
    case 3U: return APP_CAR_AIN2_D_PIN;
    default: return APP_CAR_AIN2_A_PIN;
  }
}

static bool app_car_elapsed(uint32_t now_ms, uint32_t start_ms, uint32_t duration_ms) {
  return (uint32_t)(now_ms - start_ms) >= duration_ms;
}

static bool app_car_timestamp_is_after(
  uint32_t timestamp_ms,
  uint32_t reference_ms
) {
  return (int32_t)(timestamp_ms - reference_ms) > 0;
}

static void app_car_clear_command(void) {
  Sys_Critical_Enter();
  s_car.command.valid = false;
  s_car.command.linear_speed = 0.0f;
  s_car.command.direction = 0.0f;
  s_car.command.angular_speed = 0.0f;
  Sys_Critical_Exit();
}

static void app_car_set_command(
  const joycon_input_t *input,
  float linear_speed,
  float direction,
  float angular_speed,
  uint32_t now_ms
) {
  Sys_Critical_Enter();
  s_car.command = (app_car_command_t){
    .linear_speed = linear_speed,
    .direction = direction,
    .angular_speed = angular_speed,
    .joycon_left_x = input->left_x,
    .joycon_left_y = input->left_y,
    .joycon_right_x = input->right_x,
    .time_ms = now_ms,
    .valid = true,
  };
  Sys_Critical_Exit();
}

static app_car_command_t app_car_get_command(void) {
  app_car_command_t command;

  Sys_Critical_Enter();
  command = s_car.command;
  Sys_Critical_Exit();

  return command;
}

static void app_car_set_fault(const char *reason) {
  if (!s_car.faulted) {
    s_car.faulted = true;
    SYS_LOG_ERROR("CAR", "%s", reason);
  }
}

static void app_car_enter_state(
  app_car_boot_state_t state,
  uint32_t now_ms
) {
  s_car.boot_state = state;
  s_car.state_started_ms = now_ms;
  s_car.still_since_ms = 0U;
  s_car.still_tracking = false;
}

static void app_car_stop_motion(void) {
  if (s_car.motors_enabled || s_car.estop_latched) {
    Chassis_Base_Stop(&s_chassis.base);
  }

  s_car.motors_enabled = false;
  s_car.estop_latched = false;
  app_car_clear_command();
}

static void app_car_estop(void) {
  if (!s_car.estop_latched) {
    Chassis_Base_EStop(&s_chassis.base);
    s_car.estop_latched = true;
  }

  s_car.motors_enabled = false;
  app_car_clear_command();
}

static bool app_car_motors_are_enabled(void) {
  for (uint8_t wheel = 0U; wheel < APP_CAR_WHEEL_COUNT; wheel++) {
    if (
      Dev_Motor_Base_ReadStatus(&s_motors[wheel].motor_base) !=
      DEV_MOTOR_ENABLE
    ) {
      return false;
    }
  }

  return true;
}

static void app_car_enable_motors(void) {
  for (uint8_t wheel = 0U; wheel < APP_CAR_WHEEL_COUNT; wheel++) {
    Dev_Motor_Base_Enable(&s_motors[wheel].motor_base);
  }

  s_car.motors_enabled = app_car_motors_are_enabled();
  s_car.estop_latched = false;

  if (!s_car.motors_enabled) {
    app_car_set_fault("motor enable failed");
  }
}

static bool app_car_imu_yaw_is_fresh(
  const imu_sample_t *sample,
  uint32_t now_ms
) {
  assert(sample);

  if (
    (sample->valid_mask & IMU_CHANNEL_YAW) == 0U ||
    (sample->valid_mask & IMU_CHANNEL_GYRO_Z) == 0U
  ) {
    return false;
  }

  uint32_t yaw_age_ms =
    now_ms - sample->channel_update_ms[IMU_CHANNEL_INDEX_YAW];

  return yaw_age_ms <= APP_CAR_IMU_TIMEOUT_MS;
}

static void app_car_update_boot_state(uint32_t now_ms) {
  imu_status_t imu_status = {0};
  imu_sample_t imu_sample = {0};

  Dev_IMU_Base_GetStatus(&s_imu.imu_base, &imu_status);

  switch (s_car.boot_state) {
    case APP_CAR_BOOT_RATE_CONFIG:
      if (imu_status.last_request_failed) {
        app_car_set_fault("wht101 rate config failed");
        return;
      }

      if (!imu_status.is_busy) {
        s_car.config_done_ms = now_ms;
        app_car_enter_state(APP_CAR_BOOT_WAIT_STILL, now_ms);
        SYS_LOG_INFO("IMU", "rate configured to 10 Hz");
      } else if (
        app_car_elapsed(
          now_ms,
          s_car.state_started_ms,
          APP_CAR_IMU_CONFIG_TIMEOUT_MS
        )
      ) {
        app_car_set_fault("wht101 rate config timeout");
      }
      break;

    case APP_CAR_BOOT_WAIT_STILL:
      if (!Dev_IMU_Base_GetSample(&s_imu.imu_base, &imu_sample)) {
        break;
      }

      bool has_fresh_channels =
        (imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U &&
        (imu_sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U &&
        app_car_timestamp_is_after(
          imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
          s_car.config_done_ms
        ) &&
        app_car_timestamp_is_after(
          imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_GYRO_Z],
          s_car.config_done_ms
        );
      bool is_still =
        has_fresh_channels &&
        SYS_ABS(imu_sample.gyro_dps[2]) <= APP_CAR_IMU_STILL_GYRO_DPS;

      if (is_still) {
        if (!s_car.still_tracking) {
          s_car.still_since_ms = now_ms;
          s_car.still_tracking = true;
        } else if (
          app_car_elapsed(
            now_ms,
            s_car.still_since_ms,
            APP_CAR_IMU_STILL_CONFIRM_MS
          )
        ) {
          Dev_IMU_Base_SetZero(&s_imu.imu_base);
          app_car_enter_state(APP_CAR_BOOT_ZERO_CONFIG, now_ms);
          SYS_LOG_INFO("IMU", "stationary confirmed, writing yaw zero");
        }
      } else {
        s_car.still_tracking = false;
      }

      if (
        s_car.boot_state == APP_CAR_BOOT_WAIT_STILL &&
        app_car_elapsed(
          now_ms,
          s_car.state_started_ms,
          APP_CAR_IMU_STILL_TIMEOUT_MS
        )
      ) {
        app_car_set_fault("wht101 is not stationary");
      }
      break;

    case APP_CAR_BOOT_ZERO_CONFIG:
      if (imu_status.last_request_failed) {
        app_car_set_fault("wht101 zero write failed");
        return;
      }

      if (!imu_status.is_busy) {
        s_car.zero_done_ms = now_ms;
        app_car_enter_state(APP_CAR_BOOT_WAIT_ZERO_FRAME, now_ms);
      } else if (
        app_car_elapsed(
          now_ms,
          s_car.state_started_ms,
          APP_CAR_IMU_CONFIG_TIMEOUT_MS
        )
      ) {
        app_car_set_fault("wht101 zero write timeout");
      }
      break;

    case APP_CAR_BOOT_WAIT_ZERO_FRAME:
      if (!Dev_IMU_Base_GetSample(&s_imu.imu_base, &imu_sample)) {
        break;
      }

      if (
        (imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U &&
        app_car_timestamp_is_after(
          imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
          s_car.zero_done_ms
        ) &&
        SYS_ABS(imu_sample.yaw_deg) <= APP_CAR_IMU_ZERO_RESIDUAL_DEG
      ) {
        Dev_IMU_Base_ZeroYaw(&s_imu.imu_base);
        s_car.imu_ready = true;
        app_car_enter_state(APP_CAR_BOOT_READY, now_ms);
        SYS_LOG_INFO("IMU", "sensor zero verified, car ready");
      } else if (
        app_car_elapsed(
          now_ms,
          s_car.state_started_ms,
          APP_CAR_IMU_ZERO_TIMEOUT_MS
        )
      ) {
        app_car_set_fault("wht101 zero verify timeout");
      }
      break;

    case APP_CAR_BOOT_READY:
    case APP_CAR_BOOT_FAULT:
      break;
  }
}

static void app_car_update_telemetry(uint32_t now_ms) {
  telemetry_sample_t sample = {
    .valid_mask = TELEMETRY_CHANNEL_MASK,
    .time_ms = now_ms,
  };
  imu_sample_t imu_sample = {0};
  joycon_input_t joycon_input = {0};
  app_car_command_t command = app_car_get_command();

  for (uint8_t wheel = 0U; wheel < APP_CAR_WHEEL_COUNT; wheel++) {
    sample.value[APP_CAR_TELEMETRY_TARGET_A + wheel] =
      Dev_Motor_Base_ReadTargetSpeed(&s_motors[wheel].motor_base);
    sample.value[APP_CAR_TELEMETRY_CURRENT_A + wheel] =
      Dev_Motor_Base_ReadCurrentSpeed(&s_motors[wheel].motor_base);
  }

  sample.value[APP_CAR_TELEMETRY_LINEAR_SPEED] = command.linear_speed;
  sample.value[APP_CAR_TELEMETRY_DIRECTION] = command.direction;
  sample.value[APP_CAR_TELEMETRY_ANGULAR_SPEED] = command.angular_speed;

  if (Dev_IMU_Base_GetSample(&s_imu.imu_base, &imu_sample)) {
    if ((imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U) {
      sample.value[APP_CAR_TELEMETRY_YAW] = imu_sample.yaw_deg;
    } else {
      sample.valid_mask &= ~(uint32_t)(1UL << APP_CAR_TELEMETRY_YAW);
    }

    if ((imu_sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U) {
      sample.value[APP_CAR_TELEMETRY_GYRO_Z] = imu_sample.gyro_dps[2];
    } else {
      sample.valid_mask &= ~(uint32_t)(1UL << APP_CAR_TELEMETRY_GYRO_Z);
    }
  } else {
    sample.valid_mask &=
      ~(uint32_t)(
        (1UL << APP_CAR_TELEMETRY_YAW) |
        (1UL << APP_CAR_TELEMETRY_GYRO_Z)
      );
  }

  if (Dev_JoyCon_Base_GetInput(&s_joycon.base, &joycon_input)) {
    sample.value[APP_CAR_TELEMETRY_JOYCON_LEFT_X] = joycon_input.left_x;
    sample.value[APP_CAR_TELEMETRY_JOYCON_LEFT_Y] = joycon_input.left_y;
    sample.value[APP_CAR_TELEMETRY_JOYCON_RIGHT_X] = joycon_input.right_x;
  } else {
    sample.valid_mask &=
      ~(uint32_t)(
        (1UL << APP_CAR_TELEMETRY_JOYCON_LEFT_X) |
        (1UL << APP_CAR_TELEMETRY_JOYCON_LEFT_Y) |
        (1UL << APP_CAR_TELEMETRY_JOYCON_RIGHT_X)
      );
  }

  (void)Dev_Telemetry_Base_Send(&s_justfloat.base, &sample);
}

static void app_car_log_status(uint32_t now_ms) {
  imu_status_t imu_status = {0};
  joycon_status_t joycon_status = {0};
  telemetry_status_t telemetry_status = {0};

  Dev_IMU_Base_GetStatus(&s_imu.imu_base, &imu_status);
  Dev_JoyCon_Base_GetStatus(&s_joycon.base, &joycon_status);
  Dev_Telemetry_Base_GetStatus(&s_justfloat.base, &telemetry_status);

  SYS_LOG_INFO(
    "CAR",
    "state=%u imu_err=%lu parser_err=%lu rx_ovf=%lu jf_drop=%lu hb_age=%lu",
    (unsigned int)s_car.boot_state,
    (unsigned long)imu_status.error_count,
    (unsigned long)joycon_status.invalid_frames,
    (unsigned long)joycon_status.rx_overflow_count,
    (unsigned long)telemetry_status.dropped_frames,
    (unsigned long)(
      s_car.control_heartbeat_valid
        ? now_ms - s_car.control_heartbeat_ms
        : 0U
    )
  );
}

platform_status_t App_Car_Init(void) {
  if (s_car.initialized) {
    return PLATFORM_STATUS_OK;
  }

  (void)memset(&s_car, 0, sizeof(s_car));

  Filter_Passthrough_Init(&s_imu_filter, "imu_passthrough");

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

  for (uint8_t wheel = 0U; wheel < APP_CAR_WHEEL_COUNT; wheel++) {
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
      (float)APP_CAR_PWM_MAX_DUTY,
      -(float)APP_CAR_PWM_MAX_DUTY,
      APP_CAR_PID_MAX_INTEGRAL,
      APP_CAR_PID_DERIVATIVE_TAU_S
    );

    platform_gpio_port_t stby_port =
      wheel < 2U ? APP_CAR_STBY_AB_PORT : APP_CAR_STBY_CD_PORT;
    uint16_t stby_pin =
      wheel < 2U ? APP_CAR_STBY_AB_PIN : APP_CAR_STBY_CD_PIN;

    status = Dev_Motor_TB6612_Init(
      &s_motors[wheel],
      s_motor_names[wheel],
      &s_pid[wheel].controller_base,
      &s_encoders[wheel].encoder_base,
      APP_CAR_COUNTS_PER_METER,
      APP_CAR_CONTROL_PERIOD_S,
      0.0f,
      APP_CAR_PWM_MAX_DUTY,
      PLATFORM_PWM_TIM1,
      s_pwm_channels[wheel],
      app_car_ain1_port(wheel),
      app_car_ain1_pin(wheel),
      app_car_ain2_port(wheel),
      app_car_ain2_pin(wheel),
      stby_port,
      stby_pin
    );
    if (status != PLATFORM_STATUS_OK) {
      return status;
    }
  }

  Policy_Omni_Kinematics_Init(
    &s_policy,
    "omni",
    APP_CAR_WHEEL_DISTANCE_M,
    s_wheel_theta,
    s_wheel_sign
  );

  motor_base_t *motor_h[APP_CAR_WHEEL_COUNT] = {
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
  app_car_enter_state(APP_CAR_BOOT_RATE_CONFIG, 0U);
  s_car.last_telemetry_ms = 0U;
  s_car.last_log_ms = 0U;
  s_car.initialized = true;

  SYS_LOG_INFO("CAR", "devices initialized, motors disabled");
  return PLATFORM_STATUS_OK;
}

void App_Car_ProcessInputs(uint32_t now_ms) {
  if (!s_car.initialized) {
    return;
  }

  Dev_IMU_Base_Process(&s_imu.imu_base, now_ms);
  Dev_JoyCon_UART_Process(&s_joycon, now_ms);

  if (s_car.faulted) {
    s_car.boot_state = APP_CAR_BOOT_FAULT;
    return;
  }

  if (s_car.boot_state != APP_CAR_BOOT_READY) {
    app_car_update_boot_state(now_ms);
  }
}

void App_Car_ProcessControl(uint32_t now_ms) {
  if (!s_car.initialized) {
    return;
  }

  s_car.control_heartbeat_ms = now_ms;
  s_car.control_heartbeat_valid = true;

  if (s_car.faulted) {
    app_car_estop();
    return;
  }

  if (!s_car.imu_ready || s_car.boot_state != APP_CAR_BOOT_READY) {
    app_car_stop_motion();
    return;
  }

  joycon_input_t joycon_input = {0};
  if (
    !Dev_JoyCon_Base_GetInput(&s_joycon.base, &joycon_input) ||
    (uint32_t)(now_ms - joycon_input.time_ms) > APP_CAR_COMMAND_TIMEOUT_MS
  ) {
    app_car_stop_motion();
    return;
  }

  imu_sample_t imu_sample = {0};
  if (
    !Dev_IMU_Base_GetSample(&s_imu.imu_base, &imu_sample) ||
    !app_car_imu_yaw_is_fresh(&imu_sample, now_ms)
  ) {
    app_car_stop_motion();
    return;
  }

  float left_x = SYS_CLAMP(joycon_input.left_x, -1.0f, 1.0f);
  float left_y = SYS_CLAMP(joycon_input.left_y, -1.0f, 1.0f);
  float right_x = SYS_CLAMP(joycon_input.right_x, -1.0f, 1.0f);
  float input_magnitude = sqrtf(left_x * left_x + left_y * left_y);

  if (
    input_magnitude <= APP_CAR_COMMAND_DEADBAND &&
    SYS_ABS(right_x) <= APP_CAR_COMMAND_DEADBAND
  ) {
    app_car_stop_motion();
    return;
  }

  float velocity_x = left_x * APP_CAR_MAX_LINEAR_MPS;
  float velocity_y = left_y * APP_CAR_MAX_LINEAR_MPS;
  float linear_speed = sqrtf(
    velocity_x * velocity_x +
    velocity_y * velocity_y
  );

  if (linear_speed > APP_CAR_MAX_LINEAR_MPS) {
    float scale = APP_CAR_MAX_LINEAR_MPS / linear_speed;
    velocity_x *= scale;
    velocity_y *= scale;
    linear_speed = APP_CAR_MAX_LINEAR_MPS;
  }

  float direction = 0.0f;
  if (linear_speed > 0.0f) {
    direction = atan2f(velocity_y, velocity_x);
  }

  float angular_speed =
    right_x * APP_CAR_MAX_ANGULAR_RPS;

  if (!s_car.motors_enabled) {
    app_car_enable_motors();
    if (s_car.faulted) {
      app_car_estop();
      return;
    }
  }

  Chassis_Omni_SetMotion(
    &s_chassis,
    linear_speed,
    direction,
    angular_speed
  );
  Chassis_Base_Update(&s_chassis.base, now_ms);

  if (!app_car_motors_are_enabled()) {
    s_car.motors_enabled = false;
    app_car_clear_command();
    return;
  }

  for (uint8_t wheel = 0U; wheel < APP_CAR_WHEEL_COUNT; wheel++) {
    Dev_Motor_Base_Update(&s_motors[wheel].motor_base);

    if (
      Dev_Motor_Base_ReadControllerStatus(&s_motors[wheel].motor_base) ==
      CONTROLLER_STATUS_ERROR
    ) {
      app_car_set_fault("motor controller error");
      app_car_estop();
      return;
    }
  }

  app_car_set_command(
    &joycon_input,
    linear_speed,
    direction,
    angular_speed,
    now_ms
  );
}

void App_Car_ProcessMonitor(uint32_t now_ms) {
  if (!s_car.initialized) {
    return;
  }

  if (
    s_car.control_heartbeat_valid &&
    (uint32_t)(now_ms - s_car.control_heartbeat_ms) >
      APP_CAR_HEARTBEAT_TIMEOUT_MS
  ) {
    app_car_set_fault("control heartbeat timeout");
    app_car_estop();
  } else if (s_car.faulted) {
    app_car_estop();
  }

  if (
    s_car.last_telemetry_ms == 0U ||
    (uint32_t)(now_ms - s_car.last_telemetry_ms) >=
      APP_CAR_TELEMETRY_PERIOD_MS
  ) {
    s_car.last_telemetry_ms = now_ms;
    app_car_update_telemetry(now_ms);
  }

  if (
    s_car.last_log_ms == 0U ||
    (uint32_t)(now_ms - s_car.last_log_ms) >= APP_CAR_LOG_PERIOD_MS
  ) {
    s_car.last_log_ms = now_ms;
    app_car_log_status(now_ms);
  }
}
