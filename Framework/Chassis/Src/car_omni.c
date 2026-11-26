/**
 * @file car_omni.c
 * @brief 一键装配四轮全向小车门面实现。
 * @ingroup chassis
 */

#include "car_omni.h"
#include "rtos_task.h"
#include "sys_critical.h"
#include "sys_log.h"
#include "sys_math.h"
#include "sys_snapshot.h"
#include "sys_time_window.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

#define CAR_DEFAULT_MAX_LINEAR_MPS 0.30f
#define CAR_DEFAULT_MAX_ANGULAR_RADPS 1.50f
#define CAR_DEFAULT_IMU_TIMEOUT_MS 250U
#define CAR_DEFAULT_RC_TIMEOUT_MS 300U
#define CAR_DEFAULT_DEADBAND_NORM 0.03f
#define CAR_DEFAULT_TELEMETRY_PERIOD_MS 20U
#define CAR_DEFAULT_HEARTBEAT_TIMEOUT_MS 100U
#define CAR_LOG_PERIOD_MS 1000U
#define CAR_TELEM_CHANNEL_COUNT 16U
#define CAR_FIREWATER_PRECISION 3U

static const char *const s_pid_names[KINEMATICS_OMNI_WHEELS] = {"pid_0", "pid_1", "pid_2", "pid_3"};
static const char *const s_motor_names[KINEMATICS_OMNI_WHEELS] = {"motor_0", "motor_1", "motor_2",
                                                                  "motor_3"};
static const char *const s_telem_names[CAR_TELEM_CHANNEL_COUNT] = {
    "t0", "t1", "t2", "t3", "c0", "c1", "c2", "c3", "vx", "vy", "wz", "yaw", "gz", "lx", "ly", "rx",
};

static car_omni_t *s_emergency_car = NULL;

static void car_emergency_halt_hook(void) {
  car_omni_t *car = s_emergency_car;
  if (car == NULL || !car->initialized) {
    return;
  }
  car->state = CAR_STATE_FAULT;
  car->estop_latched = true;
  car->motors_enabled = false;
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    Dev_Motor_TB6612_EmergencyHalt(&car->motors[i]);
  }
}

static void car_on_ctrl_offline(void *ctx, uint32_t now_ms) {
  (void)now_ms;
  Car_Omni_EStop((car_omni_t *)ctx, "control heartbeat timeout");
}

static void car_on_rc_offline(void *ctx, uint32_t now_ms) {
  (void)now_ms;
  car_omni_t *car = (car_omni_t *)ctx;
  SYS_LOG_WARN("CAR", "rc offline");
  Car_Omni_Stop(car);
}

static void car_on_rc_recover(void *ctx, uint32_t now_ms) {
  (void)ctx;
  (void)now_ms;
  SYS_LOG_INFO("CAR", "rc recovered");
}

static void car_on_imu_offline(void *ctx, uint32_t now_ms) {
  (void)now_ms;
  car_omni_t *car = (car_omni_t *)ctx;
  SYS_LOG_WARN("CAR", "imu offline");
  if (car != NULL && car->drive_frame == CHASSIS_FRAME_WORLD) {
    Car_Omni_Stop(car);
  }
}

static bool car_motors_all_enabled(const car_omni_t *car) {
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    if (Dev_Motor_Base_ReadStatus(&car->motors[i].base) != DEV_MOTOR_ENABLE) {
      return false;
    }
  }
  return true;
}

static bool car_enable_motors(car_omni_t *car) {
  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    Dev_Motor_Base_Enable(&car->motors[i].base);
  }
  car->motors_enabled = car_motors_all_enabled(car);
  return car->motors_enabled;
}

static bool car_twist_is_zero(const chassis_twist_t *twist) {
  return (twist->vx_body_mps == 0.0f) && (twist->vy_body_mps == 0.0f) && (twist->wz_radps == 0.0f);
}

static void car_publish_telemetry(car_omni_t *car, uint32_t now_ms) {
  if (!car->has_telemetry || car->telemetry_h == NULL ||
      !Sys_Time_TryStepPeriodicMs(&car->last_telemetry_ms, now_ms, car->telemetry_period_ms)) {
    return;
  }

  chassis_twist_t twist = {0};
  SYS_SNAPSHOT_READ(&twist, &car->desired_twist);

  telemetry_sample_t sample = {
      .valid_mask = 0xFFFFU,
      .time_ms = now_ms,
  };

  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    sample.value[i] = Dev_Motor_Base_ReadTargetSpeed(&car->motors[i].base);
    sample.value[4U + i] = Dev_Motor_Base_ReadCurrentSpeed(&car->motors[i].base);
  }
  sample.value[8] = twist.vx_body_mps;
  sample.value[9] = twist.vy_body_mps;
  sample.value[10] = twist.wz_radps;

  imu_sample_t imu_sample = {0};
  if (car->has_imu && Dev_IMU_Base_GetSample(&car->imu.base, &imu_sample)) {
    if ((imu_sample.valid_mask & IMU_CHANNEL_YAW) != 0U) {
      sample.value[11] = imu_sample.yaw_deg;
    } else {
      sample.valid_mask &= ~((uint32_t)1U << 11U);
    }
    if ((imu_sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0U) {
      sample.value[12] = imu_sample.gyro_dps[2];
    } else {
      sample.valid_mask &= ~((uint32_t)1U << 12U);
    }
  } else {
    sample.valid_mask &= ~(((uint32_t)1U << 11U) | ((uint32_t)1U << 12U));
  }

  rc_joycon_input_t rc_in = {0};
  if (car->has_rc && RC_JoyCon_GetInput(&car->rc, &rc_in)) {
    sample.value[13] = rc_in.left_x_norm;
    sample.value[14] = rc_in.left_y_norm;
    sample.value[15] = rc_in.right_x_norm;
  } else {
    sample.valid_mask &= ~(((uint32_t)1U << 13U) | ((uint32_t)1U << 14U) | ((uint32_t)1U << 15U));
  }

  (void)Dev_Telemetry_Base_Send(car->telemetry_h, &sample);
}

static void car_publish_log(car_omni_t *car, uint32_t now_ms) {
  if (!Sys_Time_TryStepPeriodicMs(&car->last_log_ms, now_ms, CAR_LOG_PERIOD_MS)) {
    return;
  }
  uint32_t imu_err = 0U;
  uint32_t rc_err = 0U;
  uint32_t telem_drop = 0U;

  if (car->has_imu) {
    imu_status_t imu_st = {0};
    Dev_IMU_Base_GetStatus(&car->imu.base, &imu_st);
    imu_err = imu_st.error_count;
  }
  if (car->has_rc) {
    rc_joycon_status_t rc_st = {0};
    RC_JoyCon_GetStatus(&car->rc, &rc_st);
    rc_err = rc_st.invalid_frames;
  }
  if (car->has_telemetry && car->telemetry_h != NULL) {
    telemetry_status_t telem_st = {0};
    Dev_Telemetry_Base_GetStatus(car->telemetry_h, &telem_st);
    telem_drop = telem_st.dropped_frames;
  }
  SYS_LOG_INFO("CAR", "state=%u imu_err=%lu rc_err=%lu telem_drop=%lu", (unsigned int)car->state,
               (unsigned long)imu_err, (unsigned long)rc_err, (unsigned long)telem_drop);
}

bsp_status_t Car_Omni_Init(car_omni_t *car, const car_omni_cfg_t *cfg) {
  if (car == NULL || cfg == NULL || !SYS_IS_POS_FINITE(cfg->control_period_s) ||
      !SYS_IS_POS_FINITE(cfg->wheel_distance_m) ||
      (cfg->drive_frame != CHASSIS_FRAME_BODY && cfg->drive_frame != CHASSIS_FRAME_WORLD)) {
    return BSP_ERR_INVALID_ARG;
  }
  if (cfg->drive_frame == CHASSIS_FRAME_WORLD && cfg->imu_wht101.huart == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  float max_linear_mps =
      (cfg->max_linear_mps > 0.0f) ? cfg->max_linear_mps : CAR_DEFAULT_MAX_LINEAR_MPS;
  float max_angular_radps =
      (cfg->max_angular_radps > 0.0f) ? cfg->max_angular_radps : CAR_DEFAULT_MAX_ANGULAR_RADPS;
  float deadband_norm = (cfg->rc_joycon.deadband_norm > 0.0f) ? cfg->rc_joycon.deadband_norm
                                                              : CAR_DEFAULT_DEADBAND_NORM;
  if (!SYS_IS_POS_FINITE(max_linear_mps) || !SYS_IS_POS_FINITE(max_angular_radps) ||
      !SYS_IS_NONNEG_FINITE(deadband_norm) || deadband_norm >= 1.0f) {
    return BSP_ERR_INVALID_ARG;
  }

  uint32_t imu_timeout_ms =
      (cfg->imu_wht101.timeout_ms > 0U) ? cfg->imu_wht101.timeout_ms : CAR_DEFAULT_IMU_TIMEOUT_MS;
  uint32_t rc_timeout_ms =
      (cfg->rc_joycon.timeout_ms > 0U) ? cfg->rc_joycon.timeout_ms : CAR_DEFAULT_RC_TIMEOUT_MS;
  uint32_t telemetry_period_ms =
      (cfg->telemetry.period_ms > 0U) ? cfg->telemetry.period_ms : CAR_DEFAULT_TELEMETRY_PERIOD_MS;
  uint32_t ctrl_hb_timeout_ms = (cfg->control_heartbeat_timeout_ms > 0U)
                                    ? cfg->control_heartbeat_timeout_ms
                                    : CAR_DEFAULT_HEARTBEAT_TIMEOUT_MS;
  if (imu_timeout_ms >= 0x80000000U || rc_timeout_ms >= 0x80000000U ||
      ctrl_hb_timeout_ms >= 0x80000000U) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(car, 0, sizeof(*car));
  car->control_period_s = cfg->control_period_s;
  car->max_linear_mps = max_linear_mps;
  car->max_angular_radps = max_angular_radps;
  car->rc_deadband_norm = deadband_norm;
  car->drive_frame = cfg->drive_frame;
  car->imu_timeout_ms = imu_timeout_ms;
  car->rc_timeout_ms = rc_timeout_ms;
  car->telemetry_period_ms = telemetry_period_ms;
  car->ctrl_hb_timeout_ms = ctrl_hb_timeout_ms;

  kinematics_omni_cfg_t kin_cfg = {.wheel_distance_m = cfg->wheel_distance_m};
  motor_base_t *motor_ptrs[KINEMATICS_OMNI_WHEELS] = {0};

  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    const car_omni_wheel_cfg_t *w = &cfg->wheels[i];
    bsp_status_t st = BSP_Encoder_Init(&car->encoders[i], &w->encoder);
    if (st != BSP_OK) {
      return st;
    }
    if (!Ctrl_PID_Init(&car->pids[i], s_pid_names[i], &w->pid)) {
      return BSP_ERR_INVALID_ARG;
    }
    const motor_tb6612_cfg_t mcfg = {
        .name = s_motor_names[i],
        .pwm = w->pwm,
        .in1 = w->in1,
        .in2 = w->in2,
        .stby = w->stby,
        .encoder = &car->encoders[i],
        .controller = &car->pids[i].base,
        .counts_per_meter = w->counts_per_meter,
        .control_period_s = cfg->control_period_s,
    };
    st = Dev_Motor_TB6612_Init(&car->motors[i], &mcfg);
    if (st != BSP_OK) {
      return st;
    }
    motor_ptrs[i] = &car->motors[i].base;
    kin_cfg.wheel_theta_rad[i] = w->theta_rad;
    kin_cfg.wheel_sign[i] = w->sign;
  }

  if (!Kinematics_Omni_Init(&car->kinematics, &kin_cfg)) {
    return BSP_ERR_INVALID_ARG;
  }

  if (cfg->imu_wht101.huart != NULL) {
    if (Filter_Passthrough_Init(&car->imu_filter, "imu_passthrough") != FILTER_STATUS_OK) {
      return BSP_ERR_HAL;
    }
    const imu_wht101_cfg_t imu_cfg = {
        .name = "wht101",
        .huart = cfg->imu_wht101.huart,
        .rx_mode = BSP_UART_RX_IT_BYTE,
        .filter = &car->imu_filter.base,
        .auto_zero_on_boot = cfg->imu_wht101.auto_zero_on_boot,
        .boot_rate = IMU_WHT101_RATE_10HZ,
    };
    bsp_status_t st = Dev_IMU_WHT101_Init(&car->imu, &imu_cfg);
    if (st != BSP_OK) {
      return st;
    }
    Dev_IMU_Base_Start(&car->imu.base);
    car->has_imu = true;
  }

  if (cfg->rc_joycon.huart != NULL) {
    const rc_joycon_cfg_t rc_cfg = {
        .huart = cfg->rc_joycon.huart,
        .rx_mode = BSP_UART_RX_IT_BYTE,
        .daemon = &car->rc_daemon,
    };
    bsp_status_t st = RC_JoyCon_Init(&car->rc, &rc_cfg);
    if (st != BSP_OK) {
      return st;
    }
    st = RC_JoyCon_Start(&car->rc);
    if (st != BSP_OK) {
      return st;
    }
    car->has_rc = true;
  }

  if (cfg->telemetry.huart != NULL) {
    bsp_status_t st = BSP_OK;
    if (cfg->telemetry.use_firewater) {
      const telem_firewater_cfg_t fw_cfg = {
          .name = "firewater",
          .huart = cfg->telemetry.huart,
          .channel_names = s_telem_names,
          .channel_count = CAR_TELEM_CHANNEL_COUNT,
          .precision = CAR_FIREWATER_PRECISION,
      };
      st = Dev_Telem_FireWater_Init(&car->telem.firewater, &fw_cfg);
      car->telemetry_h = &car->telem.firewater.base;
    } else {
      const telem_justfloat_cfg_t jf_cfg = {
          .name = "justfloat",
          .huart = cfg->telemetry.huart,
          .channel_count = CAR_TELEM_CHANNEL_COUNT,
      };
      st = Dev_Telem_JustFloat_Init(&car->telem.justfloat, &jf_cfg);
      car->telemetry_h = &car->telem.justfloat.base;
    }
    if (st != BSP_OK) {
      car->telemetry_h = NULL;
      return st;
    }
    car->has_telemetry = true;
  }

  const chassis_omni_cfg_t ch_cfg = {
      .name = "chassis_omni",
      .motors = motor_ptrs,
      .kinematics = &car->kinematics,
      .imu = car->has_imu ? &car->imu.base : NULL,
      .frame = cfg->drive_frame,
      .imu_timeout_ms = imu_timeout_ms,
  };
  if (!Chassis_Omni_Init(&car->chassis, &ch_cfg)) {
    return BSP_ERR_INVALID_ARG;
  }

  car->initialized = true;
  car->state =
      (car->has_imu && cfg->imu_wht101.auto_zero_on_boot) ? CAR_STATE_BOOTING : CAR_STATE_READY;
  s_emergency_car = car;
  RTOS_SetEmergencyHaltHook(car_emergency_halt_hook);
  return BSP_OK;
}

void Car_Omni_ProcessInputs(car_omni_t *car, uint32_t now_ms) {
  if (car == NULL || !car->initialized) {
    return;
  }

  if (car->has_rc) {
    if (!car->rc_daemon.registered) {
      const sys_daemon_cfg_t rc_dcfg = {
          .name = "rc",
          .timeout_ms = car->rc_timeout_ms,
          .init_grace_ms = car->rc_timeout_ms,
          .on_offline = car_on_rc_offline,
          .on_recover = car_on_rc_recover,
          .ctx = car,
      };
      (void)Sys_Daemon_Register(&car->rc_daemon, &rc_dcfg, now_ms);
    }
    RC_JoyCon_Process(&car->rc, now_ms);
  }

  if (car->has_imu) {
    Dev_IMU_Base_Process(&car->imu.base, now_ms);

    if (car->state == CAR_STATE_BOOTING) {
      if (Dev_IMU_WHT101_HasFault(&car->imu)) {
        Car_Omni_EStop(car, "imu boot fault");
        return;
      }
      if (Dev_IMU_WHT101_IsReady(&car->imu)) {
        car->state = CAR_STATE_READY;
      }
    }

    if (Dev_IMU_WHT101_IsReady(&car->imu)) {
      if (!car->imu_daemon.registered) {
        const sys_daemon_cfg_t imu_dcfg = {
            .name = "imu",
            .timeout_ms = car->imu_timeout_ms,
            .init_grace_ms = car->imu_timeout_ms,
            .on_offline = car_on_imu_offline,
            .on_recover = NULL,
            .ctx = car,
        };
        (void)Sys_Daemon_Register(&car->imu_daemon, &imu_dcfg, now_ms);
      }

      imu_sample_t imu_sample = {0};
      if (Dev_IMU_Base_GetSample(&car->imu.base, &imu_sample) &&
          imu_sample.sequence != car->last_imu_seq) {
        car->last_imu_seq = imu_sample.sequence;
        Sys_Daemon_Feed(&car->imu_daemon, imu_sample.sample_time_ms);
      }
    }
  }
}

void Car_Omni_SetTwist(car_omni_t *car, const chassis_twist_t *twist, uint32_t now_ms) {
  if (car == NULL || twist == NULL || !car->initialized) {
    return;
  }
  if (car->state == CAR_STATE_FAULT || car->state == CAR_STATE_BOOTING) {
    return;
  }

  chassis_twist_t clean = {
      .vx_body_mps = SYS_ISFINITE(twist->vx_body_mps) ? twist->vx_body_mps : 0.0f,
      .vy_body_mps = SYS_ISFINITE(twist->vy_body_mps) ? twist->vy_body_mps : 0.0f,
      .wz_radps = SYS_ISFINITE(twist->wz_radps) ? twist->wz_radps : 0.0f,
  };

  float linear_mag = hypotf(clean.vx_body_mps, clean.vy_body_mps);
  if (linear_mag > car->max_linear_mps && linear_mag > 0.0f) {
    float scale = car->max_linear_mps / linear_mag;
    clean.vx_body_mps *= scale;
    clean.vy_body_mps *= scale;
  }
  clean.wz_radps = SYS_CLAMP(clean.wz_radps, -car->max_angular_radps, car->max_angular_radps);

  SYS_SNAPSHOT_WRITE(&car->desired_twist, &clean);
  car->last_command_ms = now_ms;
  car->has_command = true;
}

void Car_Omni_DriveFromRC(car_omni_t *car, uint32_t now_ms) {
  if (car == NULL || !car->initialized || !car->has_rc) {
    return;
  }
  if (car->state == CAR_STATE_FAULT || car->state == CAR_STATE_BOOTING) {
    return;
  }

  rc_joycon_input_t rc_in = {0};
  if (!RC_JoyCon_GetInput(&car->rc, &rc_in) ||
      Sys_Time_IsExpiredMs(now_ms, rc_in.time_ms, car->rc_timeout_ms)) {
    Car_Omni_Stop(car);
    return;
  }

  float lx = SYS_CLAMP(rc_in.left_x_norm, -1.0f, 1.0f);
  float ly = SYS_CLAMP(rc_in.left_y_norm, -1.0f, 1.0f);
  float rx = SYS_CLAMP(rc_in.right_x_norm, -1.0f, 1.0f);
  float stick_mag = sqrtf((lx * lx) + (ly * ly));

  if (stick_mag <= car->rc_deadband_norm && SYS_ABS(rx) <= car->rc_deadband_norm) {
    Car_Omni_Stop(car);
    return;
  }

  const chassis_twist_t twist = {
      .vx_body_mps = lx * car->max_linear_mps,
      .vy_body_mps = ly * car->max_linear_mps,
      .wz_radps = rx * car->max_angular_radps,
  };
  Car_Omni_SetTwist(car, &twist, now_ms);
}

void Car_Omni_StepControl(car_omni_t *car, uint32_t now_ms) {
  if (car == NULL || !car->initialized) {
    return;
  }

  if (!car->ctrl_daemon.registered) {
    const sys_daemon_cfg_t ctrl_dcfg = {
        .name = "control_hb",
        .timeout_ms = car->ctrl_hb_timeout_ms,
        .init_grace_ms = car->ctrl_hb_timeout_ms,
        .on_offline = car_on_ctrl_offline,
        .on_recover = NULL,
        .ctx = car,
    };
    (void)Sys_Daemon_Register(&car->ctrl_daemon, &ctrl_dcfg, now_ms);
  }
  Sys_Daemon_Feed(&car->ctrl_daemon, now_ms);

  if (car->state == CAR_STATE_FAULT) {
    if (!car->estop_latched) {
      Car_Omni_EStop(car, "fault state");
    }
    return;
  }

  if (car->state == CAR_STATE_BOOTING) {
    return;
  }

  chassis_twist_t twist = {0};
  SYS_SNAPSHOT_READ(&twist, &car->desired_twist);
  if (!car->has_command || car_twist_is_zero(&twist)) {
    Car_Omni_Stop(car);
    return;
  }

  if (!car->motors_enabled && !car_enable_motors(car)) {
    Car_Omni_EStop(car, "motor enable failed");
    return;
  }

  Chassis_Base_SetMotion(&car->chassis.base, &twist);
  Chassis_Base_Update(&car->chassis.base, now_ms);
  if (!car_motors_all_enabled(car)) {
    car->motors_enabled = false;
    car->state = CAR_STATE_READY;
    return;
  }

  for (uint8_t i = 0U; i < KINEMATICS_OMNI_WHEELS; ++i) {
    Dev_Motor_Base_Update(&car->motors[i].base);
    if (Dev_Motor_Base_ReadControllerStatus(&car->motors[i].base) == CONTROLLER_STATUS_ERROR) {
      Car_Omni_EStop(car, "motor controller error");
      return;
    }
  }

  car->state = CAR_STATE_RUNNING;
}

void Car_Omni_StepService(car_omni_t *car, uint32_t now_ms) {
  if (car == NULL || !car->initialized) {
    return;
  }
  Sys_Daemon_Poll(now_ms);
  car_publish_telemetry(car, now_ms);
  car_publish_log(car, now_ms);
}

void Car_Omni_Stop(car_omni_t *car) {
  if (car == NULL || !car->initialized) {
    return;
  }
  if (car->state == CAR_STATE_FAULT) {
    return;
  }

  const chassis_twist_t zero = {0};
  SYS_SNAPSHOT_WRITE(&car->desired_twist, &zero);
  car->has_command = false;

  if (car->motors_enabled) {
    Chassis_Base_Stop(&car->chassis.base);
    car->motors_enabled = false;
  }
  if (car->state == CAR_STATE_RUNNING) {
    car->state = CAR_STATE_READY;
  }
}

void Car_Omni_EStop(car_omni_t *car, const char *reason) {
  if (car == NULL || !car->initialized) {
    return;
  }

  bool do_estop = false;
  Sys_Critical_Enter();
  if (!car->estop_latched) {
    car->estop_latched = true;
    do_estop = true;
  }
  car->state = CAR_STATE_FAULT;
  car->motors_enabled = false;
  car->has_command = false;
  Sys_Critical_Exit();

  const chassis_twist_t zero = {0};
  SYS_SNAPSHOT_WRITE(&car->desired_twist, &zero);

  if (do_estop) {
    Chassis_Base_EStop(&car->chassis.base);
    SYS_LOG_ERROR("CAR", "%s", (reason != NULL) ? reason : "estop");
  }
}

car_state_t Car_Omni_GetState(const car_omni_t *car) {
  return (car != NULL) ? car->state : CAR_STATE_FAULT;
}

chassis_base_t *Car_Omni_Chassis(car_omni_t *car) {
  return (car != NULL && car->initialized) ? &car->chassis.base : NULL;
}

imu_base_t *Car_Omni_Imu(car_omni_t *car) {
  return (car != NULL && car->initialized && car->has_imu) ? &car->imu.base : NULL;
}
