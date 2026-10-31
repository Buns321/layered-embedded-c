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
#include "system_time_window.h"
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

/**
 * @brief 返回轮毂 AIN1 控制端口。
 * @param wheel 轮位下标，有效范围 0..3，依次对应 A/B/C/D。
 * @return 对应轮位的 AIN1 GPIO 端口；非法下标回退到轮位 A 的端口。
 * @note 纯函数；调用方必须先保证 wheel 合法，回退值仅用于避免越界，不代表有效电机映射。
 */
static platform_gpio_port_t app_car_ain1_port(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN1_A_PORT;
    case 1U: return APP_CAR_AIN1_B_PORT;
    case 2U: return APP_CAR_AIN1_C_PORT;
    case 3U: return APP_CAR_AIN1_D_PORT;
    default: return APP_CAR_AIN1_A_PORT;
  }
}

/**
 * @brief 返回轮毂 AIN1 控制引脚。
 * @param wheel 轮位下标，有效范围 0..3，依次对应 A/B/C/D。
 * @return 对应轮位的 AIN1 引脚掩码；非法下标回退到轮位 A 的引脚。
 * @note 纯函数；返回值必须与 app_car_ain1_port 的轮位选择配套使用。
 */
static uint16_t app_car_ain1_pin(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN1_A_PIN;
    case 1U: return APP_CAR_AIN1_B_PIN;
    case 2U: return APP_CAR_AIN1_C_PIN;
    case 3U: return APP_CAR_AIN1_D_PIN;
    default: return APP_CAR_AIN1_A_PIN;
  }
}

/**
 * @brief 返回轮毂 AIN2 控制端口。
 * @param wheel 轮位下标，有效范围 0..3，依次对应 A/B/C/D。
 * @return 对应轮位的 AIN2 GPIO 端口；非法下标回退到轮位 A 的端口。
 * @note 纯函数；调用方必须先保证 wheel 合法，回退值仅用于避免越界。
 */
static platform_gpio_port_t app_car_ain2_port(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN2_A_PORT;
    case 1U: return APP_CAR_AIN2_B_PORT;
    case 2U: return APP_CAR_AIN2_C_PORT;
    case 3U: return APP_CAR_AIN2_D_PORT;
    default: return APP_CAR_AIN2_A_PORT;
  }
}

/**
 * @brief 返回轮毂 AIN2 控制引脚。
 * @param wheel 轮位下标，有效范围 0..3，依次对应 A/B/C/D。
 * @return 对应轮位的 AIN2 引脚掩码；非法下标回退到轮位 A 的引脚。
 * @note 纯函数；返回值必须与 app_car_ain2_port 的轮位选择配套使用。
 */
static uint16_t app_car_ain2_pin(uint8_t wheel) {
  switch (wheel) {
    case 0U: return APP_CAR_AIN2_A_PIN;
    case 1U: return APP_CAR_AIN2_B_PIN;
    case 2U: return APP_CAR_AIN2_C_PIN;
    case 3U: return APP_CAR_AIN2_D_PIN;
    default: return APP_CAR_AIN2_A_PIN;
  }
}

/**
 * @brief 用有符号差值判断时间戳是否晚于参考点。
 * @param timestamp_ms 待比较的单调毫秒时间戳。
 * @param reference_ms 参考单调毫秒时间戳。
 * @retval true 有符号差 timestamp_ms - reference_ms 大于零。
 * @retval false 时间戳等于或早于参考点。
 * @note 纯函数；要求两者间隔小于 2^31 ms，因而只能比较同一回绕窗口内的时间。
 */
static bool app_car_timestamp_is_after(
  uint32_t timestamp_ms,
  uint32_t reference_ms
) {
  return (int32_t)(timestamp_ms - reference_ms) > 0;
}

/**
 * @brief 在临界区内清除有效运动命令。
 * @post s_car.command.valid 置 false，线速度/方向/角速度清零；JoyCon 原始快照和命令时间保留。
 * @note 使用短 PRIMASK 临界区保护 ControlTask 与发布者的一致性；不修改电机或底盘硬件。
 */
static void app_car_clear_command(void) {
  Sys_Critical_Enter();
  s_car.command.valid = false;
  s_car.command.linear_speed = 0.0f;
  s_car.command.direction = 0.0f;
  s_car.command.angular_speed = 0.0f;
  Sys_Critical_Exit();
}

/**
 * @brief 在临界区内发布新的运动命令和输入快照。
 * @param input 只读 JoyCon 快照；函数只复制 left_x、left_y、right_x 三个 [-1,1] 归一化轴值。
 * @param linear_speed 世界系平动速度大小，单位 m/s，范围 0..APP_CAR_MAX_LINEAR_MPS。
 * @param direction 世界系行进方向，单位 rad，逆时针为正；有效范围 [-pi, pi]。
 * @param angular_speed 自转角速度，单位 rad/s，逆时针为正；有效范围 [-APP_CAR_MAX_ANGULAR_RPS, APP_CAR_MAX_ANGULAR_RPS]。
 * @param now_ms 命令发布时的单调毫秒时间戳。
 * @pre input 非 NULL，数值均已由 ControlTask 限幅；调用期间 input 保持有效。
 * @post 临界区内一次性覆盖运动命令、JoyCon 快照、时间戳并置 valid=true。
 * @note 仅复制数据、不访问硬件；临界区很短，不分配内存。
 */
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

/**
 * @brief 在临界区内复制当前运动命令。
 * @return 当前命令的结构体副本；内容可能 valid=false，调用方必须检查。
 * @note 使用短临界区保证整条命令一致；按值返回，不保留内部指针，不修改状态。
 */
static app_car_command_t app_car_get_command(void) {
  app_car_command_t command;

  Sys_Critical_Enter();
  command = s_car.command;
  Sys_Critical_Exit();

  return command;
}

/**
 * @brief 首次故障时锁存 faulted 并记录一条错误日志。
 * @param reason NUL 结尾的故障原因文本；非 NULL，且须在日志格式化期间保持有效。
 * @pre 只允许 App_Car 的 UARTTask、ControlTask 或 MonitorTask 调用，reason 通常为字符串字面量。
 * @post 若此前未故障，s_car.faulted 置 true 并输出一条 SYS_LOG_ERROR；已故障时不重复写状态或日志。
 * @note 无临界区保护，多任务并发进入时首次日志可能重复或交错；不分配内存、不阻塞等待，也不直接停止电机。
 * @warning reason 不得为 NULL；本函数只锁存故障，安全停机仍由调用方显式执行 app_car_estop。
 */
static void app_car_set_fault(const char *reason) {
  if (!s_car.faulted) {
    s_car.faulted = true;
    SYS_LOG_ERROR("CAR", "%s", reason);
  }
}

/**
 * @brief 切换上电状态机状态并重置静止检测计时。
 * @param state 目标启动状态，必须为 app_car_boot_state_t 的合法枚举值。
 * @param now_ms 新状态开始的单调毫秒时间戳，必须与 IMU 和任务时基一致。
 * @pre 只允许 UARTTask 在推进上电状态机时调用；调用方负责保证状态转换合法。
 * @post boot_state 与 state_started_ms 更新，still_since_ms 清零，still_tracking 置 false。
 * @note 不修改 faulted、imu_ready、电机或 IMU 设备；无锁、无硬件访问、不分配内存。
 */
static void app_car_enter_state(
  app_car_boot_state_t state,
  uint32_t now_ms
) {
  s_car.boot_state = state;
  s_car.state_started_ms = now_ms;
  s_car.still_since_ms = 0U;
  s_car.still_tracking = false;
}

/**
 * @brief 常规制动底盘、清除运动命令并复位使能标记。
 * @pre 只允许 ControlTask 调用，App_Car_Init 已成功返回，且没有其他上下文并发操作同一底盘实例。
 * @post 若电机已使能或急停曾锁存，调用 Chassis_Base_Stop 清零底盘目标并请求四轮制动；随后 motors_enabled 与 estop_latched 均置 false，共享命令失效。
 * @note 不修改 faulted 或 imu_ready，不解除故障；不需要重新上电即可重新使能电机。
 * @warning 底盘操作无锁且非原子；不得与 Chassis_Base_Update、Chassis_Base_EStop 或另一控制实例并发。
 */
static void app_car_stop_motion(void) {
  if (s_car.motors_enabled || s_car.estop_latched) {
    Chassis_Base_Stop(&s_chassis.base);
  }

  s_car.motors_enabled = false;
  s_car.estop_latched = false;
  app_car_clear_command();
}

/**
 * @brief 执行并锁存一次急停，同时使共享运动命令失效。
 * @pre 只允许 ControlTask 或 MonitorTask 调用，且底盘已完成初始化、没有其他上下文并发操作同一底盘实例。
 * @post 首次调用经 Chassis_Base_EStop 清零底盘状态、禁用四轮输出并置 estop_latched；后续调用不再重复写底盘，但仍清除 motors_enabled 和共享命令。
 * @note 无锁，四轮禁用不是跨轮原子操作；不修改 faulted 或 imu_ready，不分配内存。
 * @warning 本函数是硬件侧安全动作但无返回值，实际 STBY/GPIO 结果仍须通过电机状态另行确认。
 */
static void app_car_estop(void) {
  if (!s_car.estop_latched) {
    Chassis_Base_EStop(&s_chassis.base);
    s_car.estop_latched = true;
  }

  s_car.motors_enabled = false;
  app_car_clear_command();
}

/**
 * @brief 检查四个电机是否都处于使能状态。
 * @retval true 四个 Dev_Motor_Base_ReadStatus 均恰为 DEV_MOTOR_ENABLE。
 * @retval false 任一电机处于禁用、制动、故障或其他非 ENABLE 状态。
 * @pre 四个电机对象均已由 App_Car_Init 完成初始化。
 * @note 逐个读取状态且无锁，返回后状态可能立即变化；只能用于控制路径判断，不能替代安全联锁。
 */
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

/**
 * @brief 使能四个轮电机，按回读结果更新使能标记。
 * @pre 仅由 ControlTask 在 READY、IMU 有效且已有有效 JoyCon 输入时调用；没有其他上下文并发操作电机。
 * @post 已对四轮调用 Dev_Motor_Base_Enable 并清除 estop_latched；motors_enabled 取四轮回读的合取结果。
 * @post 若回读并非四轮全部使能，则锁存 "motor enable failed" 故障，但本函数不直接执行急停。
 * @note 有 STBY/PWM 硬件副作用；不分配内存、不等待，调用方必须在故障时继续执行 app_car_estop。
 * @warning 使能前必须确认底盘目标已清零且车轮可安全转动；驱动错误会通过 faulted 延迟暴露。
 */
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

/**
 * @brief 检查 yaw 与 gyro-Z 通道有效位，并判断 yaw 是否仍在超时窗口内。
 * @param sample 只读 IMU 快照；非 NULL，函数不接管所有权，也不修改样本。
 * @param now_ms 当前单调毫秒时间戳，必须与样本 channel_update_ms 使用同一时基。
 * @retval true YAW 与 GYRO_Z 有效位均已置位，且 yaw 通道年龄不超过 APP_CAR_IMU_TIMEOUT_MS。
 * @retval false 任一有效位缺失，或 yaw 通道年龄超过 APP_CAR_IMU_TIMEOUT_MS。
 * @pre sample 非 NULL；调试构建以 assert 检查，调用方保证生命周期覆盖本次调用。
 * @note 仅单独计算 yaw 年龄，GYRO_Z 只检查有效位；差值按 uint32 回绕安全计算，不分配内存。
 */
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

/**
 * @brief 推进 WHT101 速率配置、静止确认、设备置零和软件零偏验证状态机。
 * @param now_ms 当前单调毫秒时间戳，必须与 IMU 通道时间戳及超时配置使用同一时基。
 * @pre 只允许 UARTTask 在 App_Car_Init 成功后调用；调用时 boot_state 不应为 READY 或 FAULT。
 * @post RATE_CONFIG 在请求完成后进入 WAIT_STILL，失败或 3 s 超时会锁存故障。
 * @post WAIT_STILL 要求配置后的 yaw/gyro-Z 新帧持续静止 400 ms，随后请求设备侧置零并进入 ZERO_CONFIG；5 s 内未确认则锁存故障。
 * @post ZERO_CONFIG 在设备请求完成后记录 zero_done_ms 并等待新 yaw 帧；失败或超时锁存故障。
 * @post WAIT_ZERO_FRAME 在归零后的新 yaw 残差不超过门限时执行软件 ZeroYaw、置 imu_ready 并进入 READY；2 s 内未满足则锁存故障。
 * @note 故障分支只把 faulted 置 true，下一次 App_Car_ProcessInputs 会把 boot_state 切到 FAULT；本函数从不使能电机。
 * @warning 状态机含 IMU 设备写请求和跨任务可见的 volatile 状态，无锁；只能由单一 UARTTask 串行推进。
 */
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
        Sys_Time_HasElapsedMs(
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
          Sys_Time_HasElapsedMs(
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
        Sys_Time_HasElapsedMs(
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
        Sys_Time_HasElapsedMs(
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
        Sys_Time_HasElapsedMs(
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

/**
 * @brief 组装 16 通道车辆遥测并提交给 JustFloat。
 * @param now_ms 当前单调毫秒时间戳，写入样本作为整帧时间戳。
 * @pre 只允许 MonitorTask 调用，且 IMU、JoyCon、电机和 JustFloat 均已完成初始化。
 * @post 四轮目标/当前速度以 m/s 填入通道 0..7；命令线速度 m/s、方向 rad、角速度 rad/s 填入通道 8..10。
 * @post IMU yaw（deg）与 gyro-Z（deg/s）、JoyCon 三轴归一化值按有效位填入；对应读取失败时清除该通道有效位而不保留旧值。
 * @note SYS_LOG/注释中的“16 通道”是 JustFloat 发送宽度；样本有效掩码仍使用底层 32 通道容量。
 * @note Dev_Telemetry_Base_Send 的返回值被忽略；发送忙时 JustFloat 整帧丢弃，本函数不等待 DMA、不分配内存。
 */
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

/**
 * @brief 输出一条车辆运行状态日志。
 * @param now_ms 当前单调毫秒时间戳，用于计算 ControlTask 心跳年龄。
 * @pre 只允许 MonitorTask 在模块初始化后调用；日志后端必须已初始化且允许任务上下文写入。
 * @post 输出启动状态、IMU 错误数、JoyCon 非法帧/溢出数、JustFloat 丢帧数和心跳年龄；心跳从未有效时年龄记录为 0。
 * @note 不修改车辆状态，不访问电机或传感器硬件；SYS_LOG 为非阻塞日志门面，不分配内存。
 */
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

/**
 * @brief 初始化并静态装配小车全部设备。
 * @retval PLATFORM_STATUS_OK 设备、策略、底盘和遥测已完成静态装配；重复调用直接返回 OK。
 * @retval PLATFORM_STATUS_ERROR JoyCon 接收启动或任一底层设备装配失败。
 * @retval PLATFORM_STATUS_BUSY 某个物理 UART 的回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 底层实例或静态配置不合法。
 * @pre CubeMX 已初始化 TIM1、TIM2–TIM5、USART1–USART3、GPIO 和 NVIC。
 * @pre RTT 日志后端已按构建选项完成初始化；首次初始化须在 RTOS 调度器启动前完成。
 * @post 成功时启动 WHT101 10 Hz 输出配置并进入 RATE_CONFIG 状态，四轮保持禁用。
 * @post 失败时模块保持未初始化并返回底层错误，调用方不得继续启动控制任务。
 * @note 不分配动态内存；初始化包含同步 UART/驱动操作，重复调用不会重新装配。
 */
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

/**
 * @brief 推进 WHT101、JoyCon 和上电归零状态机。
 * @param now_ms 当前单调毫秒时间戳，必须与 ControlTask/MonitorTask 共用同一回绕时基。
 * @pre 只允许 UARTTask 在 2 ms 周期上下文中调用，且 App_Car_Init 已成功返回。
 * @post 已处理 WHT101 帧、JoyCon 帧及速率配置/静止确认/归零验证状态机。
 * @post 任一阶段失败会锁存故障，并在下一次调用进入 FAULT；成功归零后进入 READY，但本函数不会使能电机。
 * @note 未初始化时直接返回；任务上下文，不阻塞 RTOS 队列，不分配内存。
 */
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

/**
 * @brief 运行一次 100 Hz 底盘与四轮闭环控制。
 * @param now_ms 当前单调毫秒时间戳，同时用于心跳、命令和 IMU 时效判断。
 * @pre 只允许 ControlTask 调用，调用周期必须为 10 ms 且与 APP_CAR_CONTROL_PERIOD_S 一致。
 * @post 每次调用先刷新控制心跳；故障或未就绪时制动，无有效 JoyCon/IMU 或进入死区时制动。
 * @post 有效输入时启用四轮、更新底盘与 PID，并在临界区发布统一命令快照；控制器错误触发锁存急停。
 * @note 任务上下文；不分配内存，不阻塞等待，底盘/PID 输出最终由执行器限幅。
 */
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

/**
 * @brief 运行 100 Hz 监控循环：50 Hz JustFloat 遥测、1 Hz 状态日志和任务心跳检查。
 * @param now_ms 当前单调毫秒时间戳，用于心跳超时、20 ms 遥测和 1 s 日志节流。
 * @pre 只允许 MonitorTask 在 10 ms 周期上下文中调用，且 App_Car_Init 已成功返回。
 * @post 控制心跳超时锁存故障并执行急停；已有故障时继续请求急停；未初始化时直接返回。
 * @post 首次 ControlTask 刷新心跳前不判定超时；此时四轮尚未由控制路径使能。
 * @post 每 20 ms 发起一次非阻塞 16 通道 JustFloat，每 1000 ms 输出一次 RTT 状态日志。
 * @note 不分配内存；遥测忙时整帧丢弃，不等待 DMA。
 */
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
