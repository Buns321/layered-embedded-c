//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_fast_channel.c
 * @brief F103 速通赛道任务一应用实现。
 * @ingroup app
 */

#include "app_fast_channel.h"
#include "app_fast_channel_config.h"
#include "app_calibration.h"
#include "filter_base.h"
#include "filter_chain.h"
#include "filter_ekf.h"
#include "filter_imu_calibration.h"
#include "platform_gpio.h"
#include "system_critical.h"
#include "system_log.h"
#include "system_math.h"
#include "system_record_store.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define APP_PI 3.14159265358979323846f
#define APP_DEG_TO_RAD 0.01745329251994329577f
#define APP_RAD_TO_DEG 57.295779513082320876f
#define APP_FACE_COUNT 6u
#define APP_AXIS_COUNT 3u
#define APP_AXIS_X 0u
#define APP_AXIS_Y 1u
#define APP_AXIS_Z 2u
#define APP_AXIS_COMPLETE_MASK 0x07u
#define APP_TELEMETRY_CHANNEL_RAW_ACCEL_X 0u
#define APP_TELEMETRY_CHANNEL_CAL_ACCEL_X 3u
#define APP_TELEMETRY_CHANNEL_RAW_GYRO_X 6u
#define APP_TELEMETRY_CHANNEL_CAL_GYRO_X 9u
#define APP_TELEMETRY_CHANNEL_DIRECT_ROLL 12u
#define APP_TELEMETRY_CHANNEL_DIRECT_PITCH 13u
#define APP_TELEMETRY_CHANNEL_INTEGRATED_ROLL 14u
#define APP_TELEMETRY_CHANNEL_INTEGRATED_PITCH 15u
#define APP_TELEMETRY_CHANNEL_INTEGRATED_YAW 16u
#define APP_TELEMETRY_CHANNEL_EKF_ROLL 17u
#define APP_TELEMETRY_CHANNEL_EKF_PITCH 18u
#define APP_TELEMETRY_CHANNEL_EKF_YAW 19u
#define APP_TELEMETRY_CHANNEL_QUATERNION 20u
#define APP_TELEMETRY_CHANNEL_BIAS_X 24u
#define APP_TELEMETRY_CHANNEL_DT 27u
#define APP_TELEMETRY_CHANNEL_CAL_STATE 28u
#define APP_TELEMETRY_CHANNEL_FACE_MASK 29u
#define APP_TELEMETRY_CHANNEL_EKF_STATUS 30u
#define APP_TELEMETRY_CHANNEL_COMMAND_STATUS 31u

typedef struct app_calibration_record_s {
  uint32_t face_mask;
  uint32_t completed_axis_mask;
  float gyro_bias_radps[APP_AXIS_COUNT];
  float accel_offset_mps2[APP_AXIS_COUNT];
  float accel_scale[APP_AXIS_COUNT];
} app_calibration_record_t;

typedef enum app_buzzer_phase_e {
  APP_BUZZER_PHASE_OFF = 0U,
  APP_BUZZER_PHASE_SHORT_ON_1,
  APP_BUZZER_PHASE_SHORT_GAP,
  APP_BUZZER_PHASE_SHORT_ON_2,
  APP_BUZZER_PHASE_LONG_ON,
} app_buzzer_phase_t;

typedef struct app_buzzer_state_s {
  app_buzzer_phase_t phase;
  uint32_t phase_started_ms;
} app_buzzer_state_t;

typedef struct app_fast_channel_private_state_s {
  app_fast_channel_state_t calibration_state;
  app_buzzer_state_t buzzer;
  app_calibration_record_t active_record;
  app_calibration_record_t working_record;
  float face_values[APP_FACE_COUNT][APP_AXIS_COUNT];
  float face_sample_sum[APP_AXIS_COUNT];
  uint32_t face_sample_count;
  uint32_t candidate_face;
  uint32_t face_mask;
  uint32_t completed_axis_mask;
  uint32_t state_started_ms;
  uint32_t stable_started_ms;
  uint32_t face_deadline_ms;
  uint32_t inverted_since_ms;
  uint32_t last_filter_time_ms;
  uint32_t last_led_toggle_ms;
  uint32_t snapshot_sequence;
  float gyro_sum_dps[APP_AXIS_COUNT];
  float gyro_sum_sq_dps[APP_AXIS_COUNT];
  float integrated_roll_rad;
  float integrated_pitch_rad;
  float integrated_yaw_rad;
  float direct_roll_rad;
  float direct_pitch_rad;
  float last_dt_s;
  uint32_t gyro_sample_count;
  bool inverted_tracking;
  bool led_output_on;
  bool flash_record_valid;
  bool flash_write_failed;
  bool main_announced;
  bool initialized;
} app_fast_channel_private_state_t;

static mpu6050_i2c_t s_imu;
static filter_imu_calibration_t s_calibration_filter;
static filter_ekf_t s_ekf;
static filter_chain_t s_filter_chain;
static app_uart_link_t s_uart_link;
static system_record_store_t s_record_store;
static app_fast_channel_private_state_t s_app;
static app_fast_channel_snapshot_t s_snapshots[2];
static volatile uint8_t s_active_snapshot;
static app_task_metrics_t s_imu_metrics;
static uint32_t s_last_log_ms;

static const app_calibration_thresholds_t s_raw_calibration_thresholds = {
  .still_accel_min_mps2 = APP_STILL_ACCEL_MIN_MPS2,
  .still_accel_max_mps2 = APP_STILL_ACCEL_MAX_MPS2,
  .still_gyro_max_dps = APP_RAW_STILL_GYRO_MAX_DPS,
  .face_dominant_min_mps2 = APP_FACE_DOMINANT_MIN_MPS2,
  .face_cross_max_mps2 = APP_FACE_CROSS_MAX_MPS2,
};

static const app_calibration_thresholds_t s_calibrated_calibration_thresholds = {
  .still_accel_min_mps2 = APP_STILL_ACCEL_MIN_MPS2,
  .still_accel_max_mps2 = APP_STILL_ACCEL_MAX_MPS2,
  .still_gyro_max_dps = APP_STILL_GYRO_MAX_DPS,
  .face_dominant_min_mps2 = APP_FACE_DOMINANT_MIN_MPS2,
  .face_cross_max_mps2 = APP_FACE_CROSS_MAX_MPS2,
};

_Static_assert(
  sizeof(app_calibration_record_t) <= SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY,
  "calibration record exceeds record store capacity"
);
_Static_assert(
  APP_TELEMETRY_CHANNEL_COUNT <= TELEMETRY_CHANNEL_CAPACITY,
  "JustFloat channel count exceeds telemetry capacity"
);

static bool app_fastchannel_textequals(
  const char *left,
  const char *right
) {
  assert(left && right);

  while (*left != '\0' && *right != '\0') {
    char left_char = *left;
    char right_char = *right;
    if (left_char >= 'a' && left_char <= 'z') {
      left_char = (char)(left_char - 'a' + 'A');
    }
    if (right_char >= 'a' && right_char <= 'z') {
      right_char = (char)(right_char - 'a' + 'A');
    }
    if (left_char != right_char) {
      return false;
    }
    left++;
    right++;
  }

  return *left == '\0' && *right == '\0';
}

static void app_fastchannel_writebuzzer(bool enabled) {
  bool output_high = enabled
    ? APP_BUZZER_ON_LEVEL != 0U
    : APP_BUZZER_ON_LEVEL == 0U;
  Platform_GPIO_WritePin(
    APP_BUZZER_PORT,
    APP_BUZZER_PIN,
    output_high ? PLATFORM_GPIO_PIN_SET : PLATFORM_GPIO_PIN_RESET
  );
}

static void app_fastchannel_writeled(bool enabled) {
  bool output_high = enabled
    ? APP_LED_ON_LEVEL != 0U
    : APP_LED_ON_LEVEL == 0U;
  Platform_GPIO_WritePin(
    APP_LED_PORT,
    APP_LED_PIN,
    output_high ? PLATFORM_GPIO_PIN_SET : PLATFORM_GPIO_PIN_RESET
  );
}

static void app_fastchannel_startshortbuzzer(uint32_t now_ms) {
  s_app.buzzer.phase = APP_BUZZER_PHASE_SHORT_ON_1;
  s_app.buzzer.phase_started_ms = now_ms;
  app_fastchannel_writebuzzer(true);
}

static void app_fastchannel_startlongbuzzer(uint32_t now_ms) {
  s_app.buzzer.phase = APP_BUZZER_PHASE_LONG_ON;
  s_app.buzzer.phase_started_ms = now_ms;
  app_fastchannel_writebuzzer(true);
}

static void app_fastchannel_processbuzzer(uint32_t now_ms) {
  uint32_t elapsed_ms = now_ms - s_app.buzzer.phase_started_ms;

  switch (s_app.buzzer.phase) {
    case APP_BUZZER_PHASE_SHORT_ON_1:
      if (elapsed_ms >= APP_BUZZER_SHORT_ON_MS) {
        app_fastchannel_writebuzzer(false);
        s_app.buzzer.phase = APP_BUZZER_PHASE_SHORT_GAP;
        s_app.buzzer.phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_SHORT_GAP:
      if (elapsed_ms >= APP_BUZZER_SHORT_GAP_MS) {
        app_fastchannel_writebuzzer(true);
        s_app.buzzer.phase = APP_BUZZER_PHASE_SHORT_ON_2;
        s_app.buzzer.phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_SHORT_ON_2:
      if (elapsed_ms >= APP_BUZZER_SHORT_ON_MS) {
        app_fastchannel_writebuzzer(false);
        s_app.buzzer.phase = APP_BUZZER_PHASE_OFF;
        s_app.buzzer.phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_LONG_ON:
      if (elapsed_ms >= APP_BUZZER_LONG_ON_MS) {
        app_fastchannel_writebuzzer(false);
        s_app.buzzer.phase = APP_BUZZER_PHASE_OFF;
        s_app.buzzer.phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_OFF:
    default:
      app_fastchannel_writebuzzer(false);
      break;
  }
}

static void app_fastchannel_processled(uint32_t now_ms) {
  uint32_t half_period_ms =
    s_app.calibration_state == APP_FAST_CHANNEL_STATE_MAIN ? 500u : 250u;

  if (now_ms - s_app.last_led_toggle_ms >= half_period_ms) {
    s_app.last_led_toggle_ms = now_ms;
    s_app.led_output_on = !s_app.led_output_on;
    app_fastchannel_writeled(s_app.led_output_on);
  }
}

static void app_fastchannel_defaultrecord(
  app_calibration_record_t *record
) {
  assert(record);
  memset(record, 0, sizeof(*record));
  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    record->accel_scale[axis] = 1.0f;
  }
}

static void app_fastchannel_applyrecord(
  const app_calibration_record_t *record
) {
  assert(record);

  filter_imu_calibration_config_t config = {0};
  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    config.accel_offset_mps2[axis] = record->accel_offset_mps2[axis];
    config.accel_scale[axis] = record->accel_scale[axis];
    config.gyro_bias_dps[axis] =
      record->gyro_bias_radps[axis] * APP_RAD_TO_DEG;
  }
  (void)Filter_ImuCalibration_SetConfig(&s_calibration_filter, &config);
}

static bool app_fastchannel_loadcalibration(void) {
  app_calibration_record_t record;
  uint16_t version = 0u;
  uint32_t payload_size = 0u;

  system_record_store_status_t status = Sys_RecordStore_LoadLatest(
    &s_record_store,
    &version,
    &record,
    sizeof(record),
    &payload_size
  );
  if (
    status != SYSTEM_RECORD_STORE_OK ||
    version != APP_CALIBRATION_RECORD_VERSION ||
    payload_size != sizeof(record) ||
    (record.face_mask & ~((uint32_t)APP_FACE_ALL)) != 0u ||
    (record.completed_axis_mask & ~((uint32_t)APP_AXIS_COMPLETE_MASK)) != 0u
  ) {
    return false;
  }

  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    if (
      !SYS_ISFINITE(record.gyro_bias_radps[axis]) ||
      !SYS_ISFINITE(record.accel_offset_mps2[axis]) ||
      !SYS_ISFINITE(record.accel_scale[axis]) ||
      record.accel_scale[axis] < APP_ACCEL_SCALE_MIN ||
      record.accel_scale[axis] > APP_ACCEL_SCALE_MAX
    ) {
      return false;
    }
  }

  s_app.active_record = record;
  return true;
}

static bool app_fastchannel_commitcalibration(void) {
  if (
    Sys_RecordStore_Append(
      &s_record_store,
      APP_CALIBRATION_RECORD_VERSION,
      &s_app.working_record,
      sizeof(s_app.working_record)
    ) != SYSTEM_RECORD_STORE_OK
  ) {
    s_app.flash_write_failed = true;
    return false;
  }

  s_app.active_record = s_app.working_record;
  s_app.flash_record_valid = true;
  s_app.flash_write_failed = false;
  app_fastchannel_applyrecord(&s_app.active_record);
  return true;
}

static bool app_fastchannel_rawsampleisstill(
  const imu_sample_t *sample
) {
  assert(sample);
  return App_Calibration_IsStill(
    sample->accel_mps2,
    sample->gyro_dps,
    &s_raw_calibration_thresholds
  );
}

static uint32_t app_fastchannel_detectface(
  const imu_sample_t *sample
) {
  assert(sample);
  app_face_mask_t face = 0;
  return App_Calibration_DetectFace(
    sample->accel_mps2,
    &s_raw_calibration_thresholds,
    &face
  )
    ? (uint32_t)face
    : 0u;
}

static uint8_t app_fastchannel_faceindex(uint32_t face) {
  switch (face) {
    case APP_FACE_X_POSITIVE: return 0u;
    case APP_FACE_X_NEGATIVE: return 1u;
    case APP_FACE_Y_POSITIVE: return 2u;
    case APP_FACE_Y_NEGATIVE: return 3u;
    case APP_FACE_Z_POSITIVE: return 4u;
    case APP_FACE_Z_NEGATIVE: return 5u;
    default: return UINT8_MAX;
  }
}

static uint8_t app_fastchannel_faceaxis(uint32_t face) {
  if (
    face == APP_FACE_X_POSITIVE ||
    face == APP_FACE_X_NEGATIVE
  ) {
    return APP_AXIS_X;
  }
  if (
    face == APP_FACE_Y_POSITIVE ||
    face == APP_FACE_Y_NEGATIVE
  ) {
    return APP_AXIS_Y;
  }
  return APP_AXIS_Z;
}

static bool app_fastchannel_acceptface(
  uint32_t face,
  const float accel_mean[APP_AXIS_COUNT]
) {
  assert(accel_mean);

  uint8_t face_index = app_fastchannel_faceindex(face);
  uint8_t axis = app_fastchannel_faceaxis(face);
  if (face_index == UINT8_MAX) {
    return false;
  }

  uint32_t opposite_face = 0u;
  switch (face) {
    case APP_FACE_X_POSITIVE:
      opposite_face = APP_FACE_X_NEGATIVE;
      break;
    case APP_FACE_X_NEGATIVE:
      opposite_face = APP_FACE_X_POSITIVE;
      break;
    case APP_FACE_Y_POSITIVE:
      opposite_face = APP_FACE_Y_NEGATIVE;
      break;
    case APP_FACE_Y_NEGATIVE:
      opposite_face = APP_FACE_Y_POSITIVE;
      break;
    case APP_FACE_Z_POSITIVE:
      opposite_face = APP_FACE_Z_NEGATIVE;
      break;
    case APP_FACE_Z_NEGATIVE:
      opposite_face = APP_FACE_Z_POSITIVE;
      break;
    default:
      return false;
  }

  float offset = s_app.working_record.accel_offset_mps2[axis];
  float scale = s_app.working_record.accel_scale[axis];
  if ((s_app.face_mask & opposite_face) != 0u) {
    uint8_t opposite_index = app_fastchannel_faceindex(opposite_face);
    bool current_is_positive =
      face == APP_FACE_X_POSITIVE ||
      face == APP_FACE_Y_POSITIVE ||
      face == APP_FACE_Z_POSITIVE;

    const float *positive_values = current_is_positive
      ? accel_mean
      : s_app.face_values[opposite_index];
    const float *negative_values = current_is_positive
      ? s_app.face_values[opposite_index]
      : accel_mean;
    if (!App_Calibration_ComputeAccelAxis(
          positive_values,
          negative_values,
          axis,
          APP_GRAVITY_MPS2,
          APP_ACCEL_CALIBRATION_MIN_SPAN_MPS2,
          APP_ACCEL_SCALE_MIN,
          APP_ACCEL_SCALE_MAX,
          &offset,
          &scale
        )) {
      return false;
    }
    s_app.completed_axis_mask |= (uint32_t)1u << axis;
  }

  for (uint8_t component = 0u; component < APP_AXIS_COUNT; component++) {
    s_app.face_values[face_index][component] = accel_mean[component];
  }
  s_app.face_mask |= face;
  s_app.working_record.face_mask = s_app.face_mask;
  s_app.working_record.completed_axis_mask = s_app.completed_axis_mask;
  s_app.working_record.accel_offset_mps2[axis] = offset;
  s_app.working_record.accel_scale[axis] = scale;
  return true;
}

static void app_fastchannel_entergyrocalibration(uint32_t now_ms) {
  s_app.calibration_state = APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION;
  s_app.state_started_ms = now_ms;
  s_app.gyro_sample_count = 0u;
  memset(s_app.gyro_sum_dps, 0, sizeof(s_app.gyro_sum_dps));
  memset(s_app.gyro_sum_sq_dps, 0, sizeof(s_app.gyro_sum_sq_dps));
}

static void app_fastchannel_entermain(
  uint32_t now_ms,
  const char *reason
) {
  if (s_app.calibration_state == APP_FAST_CHANNEL_STATE_MAIN) {
    return;
  }

  s_app.working_record = s_app.active_record;
  s_app.face_mask = s_app.active_record.face_mask;
  s_app.completed_axis_mask = s_app.active_record.completed_axis_mask;
  app_fastchannel_applyrecord(&s_app.active_record);
  Filter_Base_Reset(&s_ekf.filter_base);
  s_app.calibration_state = APP_FAST_CHANNEL_STATE_MAIN;
  s_app.state_started_ms = now_ms;

  if (!s_app.main_announced) {
    s_app.main_announced = true;
    app_fastchannel_startlongbuzzer(now_ms);
  }
  SYS_LOG_INFO("APP", "main started: %s", reason);
}

static void app_fastchannel_processinitialpose(
  uint32_t now_ms,
  const imu_sample_t *raw_sample
) {
  assert(raw_sample);

  bool inverted =
    app_fastchannel_rawsampleisstill(raw_sample) &&
    raw_sample->accel_mps2[APP_AXIS_Z] <= APP_INVERTED_AZ_MAX_MPS2;

  if (inverted) {
    if (!s_app.inverted_tracking) {
      s_app.inverted_tracking = true;
      s_app.inverted_since_ms = now_ms;
    }
  } else {
    s_app.inverted_tracking = false;
  }

  if (
    s_app.inverted_tracking &&
    (uint32_t)(now_ms - s_app.inverted_since_ms) >=
      APP_INITIAL_POSE_CHECK_MS
  ) {
    app_fastchannel_entermain(now_ms, "initial -Z skip");
    return;
  }

  if (
    (uint32_t)(now_ms - s_app.state_started_ms) >=
    APP_INITIAL_POSE_CHECK_MS
  ) {
    app_fastchannel_entergyrocalibration(now_ms);
  }
}

static void app_fastchannel_processgyrocalibration(
  uint32_t now_ms,
  const imu_sample_t *raw_sample
) {
  assert(raw_sample);

  if ((uint32_t)(now_ms - s_app.state_started_ms) >=
      APP_CALIBRATION_FACE_TIMEOUT_MS) {
    app_fastchannel_entermain(now_ms, "gyro calibration timeout");
    return;
  }

  if (!app_fastchannel_rawsampleisstill(raw_sample)) {
    s_app.gyro_sample_count = 0u;
    memset(s_app.gyro_sum_dps, 0, sizeof(s_app.gyro_sum_dps));
    memset(s_app.gyro_sum_sq_dps, 0, sizeof(s_app.gyro_sum_sq_dps));
    return;
  }

  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    float value = raw_sample->gyro_dps[axis];
    s_app.gyro_sum_dps[axis] += value;
    s_app.gyro_sum_sq_dps[axis] += value * value;
  }
  s_app.gyro_sample_count++;

  if (s_app.gyro_sample_count < APP_GYRO_CALIBRATION_SAMPLES) {
    return;
  }

  float mean_dps[APP_AXIS_COUNT];
  float count = (float)s_app.gyro_sample_count;
  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    mean_dps[axis] = s_app.gyro_sum_dps[axis] / count;
    float variance =
      s_app.gyro_sum_sq_dps[axis] / count -
      mean_dps[axis] * mean_dps[axis];
    if (variance < 0.0f) {
      variance = 0.0f;
    }
    if (sqrtf(variance) > APP_GYRO_CALIBRATION_MAX_STD_DPS) {
      s_app.gyro_sample_count = 0u;
      memset(s_app.gyro_sum_dps, 0, sizeof(s_app.gyro_sum_dps));
      memset(s_app.gyro_sum_sq_dps, 0, sizeof(s_app.gyro_sum_sq_dps));
      return;
    }
  }

  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    s_app.working_record.gyro_bias_radps[axis] =
      mean_dps[axis] * APP_DEG_TO_RAD;
  }
  app_fastchannel_applyrecord(&s_app.working_record);

  s_app.calibration_state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
  s_app.state_started_ms = now_ms;
  s_app.face_deadline_ms =
    now_ms + APP_CALIBRATION_FACE_TIMEOUT_MS;
  SYS_LOG_INFO("CAL", "gyro bias calibrated");
}

static bool app_fastchannel_facetimedout(uint32_t now_ms) {
  return (int32_t)(now_ms - s_app.face_deadline_ms) >= 0;
}

static void app_fastchannel_resetfacesearch(uint32_t now_ms) {
  s_app.calibration_state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
  s_app.state_started_ms = now_ms;
  s_app.face_sample_count = 0u;
  s_app.candidate_face = 0u;
  s_app.face_deadline_ms =
    now_ms + APP_CALIBRATION_FACE_TIMEOUT_MS;
}

static void app_fastchannel_processfacesearch(
  uint32_t now_ms,
  const imu_sample_t *raw_sample,
  const imu_sample_t *calibrated_sample
) {
  assert(raw_sample && calibrated_sample);

  if (app_fastchannel_facetimedout(now_ms)) {
    app_fastchannel_entermain(now_ms, "face calibration timeout");
    return;
  }

  uint32_t detected_face = app_fastchannel_detectface(raw_sample);
  bool still = App_Calibration_IsStill(
    raw_sample->accel_mps2,
    calibrated_sample->gyro_dps,
    &s_calibrated_calibration_thresholds
  );

  switch (s_app.calibration_state) {
    case APP_FAST_CHANNEL_STATE_WAIT_FACE:
      if (
        detected_face == 0u ||
        (s_app.face_mask & detected_face) != 0u ||
        !still
      ) {
        return;
      }
      s_app.candidate_face = detected_face;
      s_app.stable_started_ms = now_ms;
      s_app.calibration_state =
        APP_FAST_CHANNEL_STATE_STABILIZE_FACE;
      break;

    case APP_FAST_CHANNEL_STATE_STABILIZE_FACE:
      if (detected_face != s_app.candidate_face || !still) {
        s_app.calibration_state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
        return;
      }
      if ((uint32_t)(now_ms - s_app.stable_started_ms) >=
          APP_FACE_STABLE_MS) {
        s_app.face_sample_count = 0u;
        memset(
          s_app.face_sample_sum,
          0,
          sizeof(s_app.face_sample_sum)
        );
        s_app.calibration_state =
          APP_FAST_CHANNEL_STATE_SAMPLE_FACE;
      }
      break;

    case APP_FAST_CHANNEL_STATE_SAMPLE_FACE:
      if (detected_face != s_app.candidate_face || !still) {
        s_app.calibration_state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
        return;
      }
      for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
        s_app.face_sample_sum[axis] += raw_sample->accel_mps2[axis];
      }
      s_app.face_sample_count++;
      if (s_app.face_sample_count >= APP_FACE_SAMPLE_COUNT) {
        float mean[APP_AXIS_COUNT];
        for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
          mean[axis] =
            s_app.face_sample_sum[axis] /
            (float)s_app.face_sample_count;
        }

        if (!app_fastchannel_acceptface(
              s_app.candidate_face,
              mean
            )) {
          SYS_LOG_WARN("CAL", "face rejected: invalid span");
          app_fastchannel_resetfacesearch(now_ms);
          return;
        }

        bool stored = app_fastchannel_commitcalibration();
        bool complete = s_app.face_mask == APP_FACE_ALL;
        if (complete) {
          app_fastchannel_entermain(
            now_ms,
            stored ? "calibration complete" : "calibration flash fallback"
          );
        } else {
          if (stored) {
            app_fastchannel_startshortbuzzer(now_ms);
          }
          app_fastchannel_resetfacesearch(now_ms);
        }
      }
      break;

    default:
      break;
  }
}

static float app_fastchannel_wrapradians(float angle_rad) {
  while (angle_rad > APP_PI) {
    angle_rad -= 2.0f * APP_PI;
  }
  while (angle_rad < -APP_PI) {
    angle_rad += 2.0f * APP_PI;
  }
  return angle_rad;
}

static void app_fastchannel_updatereference(
  uint32_t now_ms,
  const imu_sample_t *sample
) {
  assert(sample);

  float dt_s = s_app.last_dt_s;
  if (s_app.last_filter_time_ms != 0u) {
    dt_s = (float)(now_ms - s_app.last_filter_time_ms) * 0.001f;
    dt_s = SYS_CLAMP(dt_s, APP_EKF_MIN_DT_S, APP_EKF_MAX_DT_S);
  }
  s_app.last_filter_time_ms = now_ms;
  s_app.last_dt_s = dt_s;

  if ((sample->valid_mask & IMU_CHANNEL_ACCEL_Z) != 0u) {
    float roll = atan2f(
      sample->accel_mps2[1],
      sample->accel_mps2[2]
    );
    float pitch = atan2f(
      -sample->accel_mps2[0],
      sqrtf(
        sample->accel_mps2[1] * sample->accel_mps2[1] +
        sample->accel_mps2[2] * sample->accel_mps2[2]
      )
    );
    if (SYS_ISFINITE(roll) && SYS_ISFINITE(pitch)) {
      s_app.direct_roll_rad = roll;
      s_app.direct_pitch_rad = pitch;
    }
  }

  if (
    (sample->valid_mask & IMU_CHANNEL_GYRO_X) != 0u &&
    (sample->valid_mask & IMU_CHANNEL_GYRO_Y) != 0u &&
    (sample->valid_mask & IMU_CHANNEL_GYRO_Z) != 0u
  ) {
    s_app.integrated_roll_rad = app_fastchannel_wrapradians(
      s_app.integrated_roll_rad +
      sample->gyro_dps[0] * APP_DEG_TO_RAD * dt_s
    );
    s_app.integrated_pitch_rad = app_fastchannel_wrapradians(
      s_app.integrated_pitch_rad +
      sample->gyro_dps[1] * APP_DEG_TO_RAD * dt_s
    );
    s_app.integrated_yaw_rad = app_fastchannel_wrapradians(
      s_app.integrated_yaw_rad +
      sample->gyro_dps[2] * APP_DEG_TO_RAD * dt_s
    );
  }
}

static void app_fastchannel_publishsnapshot(
  uint32_t now_ms,
  bool raw_valid,
  bool calibrated_valid
) {
  uint8_t next_index = (uint8_t)(s_active_snapshot ^ 1u);
  app_fast_channel_snapshot_t *snapshot = &s_snapshots[next_index];
  memset(snapshot, 0, sizeof(*snapshot));

  if (
    raw_valid &&
    Dev_MPU6050_I2C_GetRawSample(&s_imu, &snapshot->raw_sample)
  ) {
    snapshot->raw_valid = true;
  }
  if (
    calibrated_valid &&
    Dev_IMU_Base_GetSample(&s_imu.imu_base, &snapshot->calibrated_sample)
  ) {
    snapshot->calibrated_valid = true;
  }
  Filter_Ekf_GetState(&s_ekf, &snapshot->ekf_state);

  snapshot->calibration_state = s_app.calibration_state;
  snapshot->face_mask = s_app.face_mask;
  snapshot->completed_axis_mask = s_app.completed_axis_mask;
  snapshot->sequence = ++s_app.snapshot_sequence;
  snapshot->direct_roll_rad = s_app.direct_roll_rad;
  snapshot->direct_pitch_rad = s_app.direct_pitch_rad;
  snapshot->integrated_roll_rad = s_app.integrated_roll_rad;
  snapshot->integrated_pitch_rad = s_app.integrated_pitch_rad;
  snapshot->integrated_yaw_rad = s_app.integrated_yaw_rad;
  snapshot->dt_s = s_app.last_dt_s;
  snapshot->flash_record_valid = s_app.flash_record_valid;
  snapshot->flash_write_failed = s_app.flash_write_failed;

  app_uart_link_status_t link_status;
  App_UART_LinkGetStatus(&s_uart_link, &link_status);
  snapshot->command_status = link_status.last_command_status;

  (void)now_ms;
  Sys_Critical_Enter();
  s_active_snapshot = next_index;
  Sys_Critical_Exit();
}

static bool app_fastchannel_applycommand(
  const app_uart_command_t *command,
  void *context
) {
  (void)context;
  assert(command);

  if (command->kind == APP_UART_COMMAND_RESET_EKF) {
    Filter_Ekf_ResetParameters(&s_ekf);
    return true;
  }
  if (command->kind != APP_UART_COMMAND_SET) {
    return false;
  }

  filter_ekf_parameter_t parameter;
  if (app_fastchannel_textequals(command->name, "Q_ANGLE")) {
    parameter = FILTER_EKF_PARAM_ANGLE_PROCESS_NOISE;
  } else if (app_fastchannel_textequals(command->name, "Q_BIAS")) {
    parameter = FILTER_EKF_PARAM_BIAS_PROCESS_NOISE;
  } else if (app_fastchannel_textequals(command->name, "R_ACCEL")) {
    parameter = FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE;
  } else if (app_fastchannel_textequals(command->name, "P0_ANGLE")) {
    parameter = FILTER_EKF_PARAM_INITIAL_ANGLE_VARIANCE;
  } else if (app_fastchannel_textequals(command->name, "P0_BIAS")) {
    parameter = FILTER_EKF_PARAM_INITIAL_BIAS_VARIANCE;
  } else if (app_fastchannel_textequals(command->name, "ACCEL_MIN")) {
    parameter = FILTER_EKF_PARAM_ACCEL_NORM_MIN;
  } else if (app_fastchannel_textequals(command->name, "ACCEL_MAX")) {
    parameter = FILTER_EKF_PARAM_ACCEL_NORM_MAX;
  } else if (app_fastchannel_textequals(command->name, "GYRO_LIMIT")) {
    parameter = FILTER_EKF_PARAM_GYRO_LIMIT;
  } else if (app_fastchannel_textequals(command->name, "BIAS_LIMIT")) {
    parameter = FILTER_EKF_PARAM_BIAS_LIMIT;
  } else if (app_fastchannel_textequals(command->name, "DT_MIN")) {
    parameter = FILTER_EKF_PARAM_MIN_DT;
  } else if (app_fastchannel_textequals(command->name, "DT_MAX")) {
    parameter = FILTER_EKF_PARAM_MAX_DT;
  } else {
    return false;
  }

  return Filter_Ekf_SetParameter(&s_ekf, parameter, command->value);
}

platform_status_t App_FastChannel_Init(void) {
  if (s_app.initialized) {
    return PLATFORM_STATUS_OK;
  }

  memset(&s_app, 0, sizeof(s_app));
  memset(s_snapshots, 0, sizeof(s_snapshots));
  memset(&s_imu_metrics, 0, sizeof(s_imu_metrics));

  app_fastchannel_writebuzzer(false);
  app_fastchannel_writeled(false);
  app_fastchannel_defaultrecord(&s_app.active_record);
  s_app.working_record = s_app.active_record;

  const filter_imu_calibration_config_t default_calibration = {
    .accel_scale = {1.0f, 1.0f, 1.0f},
  };
  if (!Filter_ImuCalibration_Init(
        &s_calibration_filter,
        "imu_calibration",
        &default_calibration
      )) {
    return PLATFORM_STATUS_ERROR;
  }

  const filter_ekf_config_t ekf_config = {
    .angle_process_noise_rad2 =
      APP_EKF_ANGLE_PROCESS_NOISE_RAD2,
    .bias_process_noise_radps2 =
      APP_EKF_BIAS_PROCESS_NOISE_RADPS2,
    .accel_measurement_noise_mps2_2 =
      APP_EKF_ACCEL_MEASUREMENT_NOISE_MPS2_2,
    .initial_angle_variance_rad2 =
      APP_EKF_INITIAL_ANGLE_VARIANCE_RAD2,
    .initial_bias_variance_radps2 =
      APP_EKF_INITIAL_BIAS_VARIANCE_RADPS2,
    .accel_norm_min_mps2 = APP_EKF_ACCEL_NORM_MIN_MPS2,
    .accel_norm_max_mps2 = APP_EKF_ACCEL_NORM_MAX_MPS2,
    .gyro_limit_radps = APP_EKF_GYRO_LIMIT_RADPS,
    .bias_limit_radps = APP_EKF_BIAS_LIMIT_RADPS,
    .min_dt_s = APP_EKF_MIN_DT_S,
    .max_dt_s = APP_EKF_MAX_DT_S,
  };
  if (!Filter_Ekf_Init(&s_ekf, "ekf", &ekf_config)) {
    return PLATFORM_STATUS_ERROR;
  }

  filter_base_t *filter_stages[] = {
    &s_calibration_filter.filter_base,
    &s_ekf.filter_base,
  };
  Filter_Chain_Init(
    &s_filter_chain,
    "imu_chain",
    filter_stages,
    2u
  );

  system_record_store_status_t store_status = Sys_RecordStore_Init(
    &s_record_store,
    PLATFORM_FLASH_1,
    APP_CALIBRATION_FLASH_ADDRESS,
    APP_CALIBRATION_FLASH_PAGE_SIZE,
    APP_CALIBRATION_RECORD_MAGIC
  );
  if (store_status != SYSTEM_RECORD_STORE_OK) {
    s_app.flash_write_failed = true;
    SYS_LOG_ERROR("FLASH", "record store init failed");
  } else {
    s_app.flash_record_valid = app_fastchannel_loadcalibration();
  }
  app_fastchannel_applyrecord(&s_app.active_record);

  platform_status_t status = App_UART_LinkInit(
    &s_uart_link,
    "uart1_link",
    app_fastchannel_applycommand,
    NULL
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  mpu6050_config_t imu_config = {
    .i2c = APP_IMU_I2C_INSTANCE,
    .address = (uint8_t)APP_IMU_I2C_ADDRESS,
    .dlpf = APP_IMU_DLPF,
    .gyro_range = APP_IMU_GYRO_RANGE,
    .accel_range = APP_IMU_ACCEL_RANGE,
    .sample_rate_hz = APP_IMU_SAMPLE_RATE_HZ,
    .calibration_samples = 0u,
    .i2c_timeout_ms = APP_IMU_I2C_TIMEOUT_MS,
    .axis_source = {0u, 1u, 2u},
    .axis_sign = {1, 1, 1},
  };

  status = Dev_MPU6050_I2C_Init(
    &s_imu,
    "mpu6050",
    &imu_config,
    &s_filter_chain.filter_base
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }
  Dev_IMU_Base_Start(&s_imu.imu_base);

  status = App_UART_LinkStart(&s_uart_link);
  if (status != PLATFORM_STATUS_OK) {
    SYS_LOG_WARN("UART", "receive start deferred");
  }

  s_app.calibration_state =
    APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK;
  s_app.state_started_ms = 0u;
  s_app.last_dt_s = (float)APP_IMU_TASK_PERIOD_MS * 0.001f;
  s_app.initialized = true;

  SYS_LOG_INFO(
    "APP",
    "initialized flash=%u",
    s_app.flash_record_valid ? 1u : 0u
  );
  return PLATFORM_STATUS_OK;
}

void App_FastChannel_ProcessImu(uint32_t now_ms) {
  if (!s_app.initialized) {
    return;
  }

  if (s_app.state_started_ms == 0u) {
    s_app.state_started_ms = now_ms;
  }

  Dev_IMU_Base_Process(&s_imu.imu_base, now_ms);

  imu_sample_t raw_sample = {0};
  imu_sample_t calibrated_sample = {0};
  bool raw_valid =
    Dev_IMU_Base_GetRawSample(&s_imu.imu_base, &raw_sample);
  bool calibrated_valid =
    Dev_IMU_Base_GetSample(&s_imu.imu_base, &calibrated_sample);

  if (raw_valid) {
    switch (s_app.calibration_state) {
      case APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK:
        app_fastchannel_processinitialpose(now_ms, &raw_sample);
        break;
      case APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION:
        app_fastchannel_processgyrocalibration(now_ms, &raw_sample);
        break;
      case APP_FAST_CHANNEL_STATE_WAIT_FACE:
      case APP_FAST_CHANNEL_STATE_STABILIZE_FACE:
      case APP_FAST_CHANNEL_STATE_SAMPLE_FACE:
        app_fastchannel_processfacesearch(
          now_ms,
          &raw_sample,
          &calibrated_sample
        );
        break;
      case APP_FAST_CHANNEL_STATE_MAIN:
      case APP_FAST_CHANNEL_STATE_ERROR:
      case APP_FAST_CHANNEL_STATE_UNINITIALIZED:
      default:
        break;
    }
  } else if (
    s_app.calibration_state != APP_FAST_CHANNEL_STATE_MAIN &&
    (uint32_t)(now_ms - s_app.state_started_ms) >=
      APP_CALIBRATION_FACE_TIMEOUT_MS
  ) {
    app_fastchannel_entermain(now_ms, "IMU unavailable");
  }

  if (calibrated_valid) {
    app_fastchannel_updatereference(now_ms, &calibrated_sample);
  }

  app_fastchannel_publishsnapshot(
    now_ms,
    raw_valid,
    calibrated_valid
  );
  app_fastchannel_processbuzzer(now_ms);
  app_fastchannel_processled(now_ms);
}

static void app_fastchannel_setchannelvalid(
  telemetry_sample_t *sample,
  uint32_t channel,
  bool valid
) {
  assert(sample && channel < TELEMETRY_CHANNEL_CAPACITY);
  uint32_t mask = (uint32_t)1u << channel;
  if (valid) {
    sample->valid_mask |= mask;
  } else {
    sample->valid_mask &= ~mask;
  }
}

void App_FastChannel_ProcessTelemetry(uint32_t now_ms) {
  if (!s_app.initialized) {
    return;
  }

  App_UART_LinkProcessCommands(&s_uart_link, now_ms);

  app_fast_channel_snapshot_t snapshot;
  App_FastChannel_GetSnapshot(&snapshot);

  telemetry_sample_t telemetry = {
    .valid_mask = TELEMETRY_CHANNEL_MASK,
    .time_ms = now_ms,
  };

  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    telemetry.value[APP_TELEMETRY_CHANNEL_RAW_ACCEL_X + axis] =
      snapshot.raw_sample.accel_mps2[axis];
    telemetry.value[APP_TELEMETRY_CHANNEL_CAL_ACCEL_X + axis] =
      snapshot.calibrated_sample.accel_mps2[axis];
    telemetry.value[APP_TELEMETRY_CHANNEL_RAW_GYRO_X + axis] =
      snapshot.raw_sample.gyro_dps[axis] * APP_DEG_TO_RAD;
    telemetry.value[APP_TELEMETRY_CHANNEL_CAL_GYRO_X + axis] =
      snapshot.calibrated_sample.gyro_dps[axis] * APP_DEG_TO_RAD;
    app_fastchannel_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_RAW_ACCEL_X + axis,
      snapshot.raw_valid
    );
    app_fastchannel_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_RAW_GYRO_X + axis,
      snapshot.raw_valid
    );
    app_fastchannel_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_CAL_ACCEL_X + axis,
      snapshot.calibrated_valid
    );
    app_fastchannel_setchannelvalid(
      &telemetry,
      APP_TELEMETRY_CHANNEL_CAL_GYRO_X + axis,
      snapshot.calibrated_valid
    );
    telemetry.value[APP_TELEMETRY_CHANNEL_BIAS_X + axis] =
      snapshot.ekf_state.gyro_bias_radps[axis];
  }

  telemetry.value[APP_TELEMETRY_CHANNEL_DIRECT_ROLL] =
    snapshot.direct_roll_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_DIRECT_PITCH] =
    snapshot.direct_pitch_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_INTEGRATED_ROLL] =
    snapshot.integrated_roll_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_INTEGRATED_PITCH] =
    snapshot.integrated_pitch_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_INTEGRATED_YAW] =
    snapshot.integrated_yaw_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_ROLL] =
    snapshot.ekf_state.roll_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_PITCH] =
    snapshot.ekf_state.pitch_rad;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_YAW] =
    snapshot.ekf_state.yaw_rad;
  for (uint8_t index = 0u; index < 4u; index++) {
    telemetry.value[APP_TELEMETRY_CHANNEL_QUATERNION + index] =
      snapshot.ekf_state.quaternion[index];
  }
  for (uint8_t channel = APP_TELEMETRY_CHANNEL_DIRECT_ROLL;
       channel <= APP_TELEMETRY_CHANNEL_INTEGRATED_YAW;
       channel++) {
    app_fastchannel_setchannelvalid(
      &telemetry,
      channel,
      snapshot.calibrated_valid
    );
  }
  for (uint8_t channel = APP_TELEMETRY_CHANNEL_EKF_ROLL;
       channel <= APP_TELEMETRY_CHANNEL_QUATERNION + 3u;
       channel++) {
    app_fastchannel_setchannelvalid(
      &telemetry,
      channel,
      snapshot.ekf_state.valid
    );
  }
  for (uint8_t channel = APP_TELEMETRY_CHANNEL_BIAS_X;
       channel < APP_TELEMETRY_CHANNEL_BIAS_X + 3u;
       channel++) {
    app_fastchannel_setchannelvalid(
      &telemetry,
      channel,
      snapshot.ekf_state.valid
    );
  }
  telemetry.value[APP_TELEMETRY_CHANNEL_DT] = snapshot.dt_s;
  telemetry.value[APP_TELEMETRY_CHANNEL_CAL_STATE] =
    (float)snapshot.calibration_state;
  telemetry.value[APP_TELEMETRY_CHANNEL_FACE_MASK] =
    (float)snapshot.face_mask;
  telemetry.value[APP_TELEMETRY_CHANNEL_EKF_STATUS] =
    snapshot.ekf_state.placeholder
      ? 100.0f
      : (snapshot.ekf_state.valid ? 1.0f : 0.0f);
  app_uart_link_status_t command_status;
  App_UART_LinkGetStatus(&s_uart_link, &command_status);
  telemetry.value[APP_TELEMETRY_CHANNEL_COMMAND_STATUS] =
    (float)command_status.last_command_status;

  (void)App_UART_LinkSendTelemetry(&s_uart_link, &telemetry);

  if (
    s_last_log_ms == 0u ||
    (uint32_t)(now_ms - s_last_log_ms) >= 1000u
  ) {
    s_last_log_ms = now_ms;
    SYS_LOG_INFO(
      "RUN",
      "state=%u face=%u flash=%u cmd=%u drop=%lu us=%lu jitter=%lu stack=%lu",
      (unsigned int)snapshot.calibration_state,
      (unsigned int)snapshot.face_mask,
      snapshot.flash_record_valid ? 1u : 0u,
      (unsigned int)command_status.last_command_status,
      (unsigned long)command_status.telemetry.dropped_frames,
      (unsigned long)s_imu_metrics.execution_us,
      (unsigned long)s_imu_metrics.period_jitter_ms,
      (unsigned long)s_imu_metrics.stack_free_bytes
    );
  }
}

void App_FastChannel_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
) {
  assert(snapshot);

  Sys_Critical_Enter();
  *snapshot = s_snapshots[s_active_snapshot];
  Sys_Critical_Exit();
}

void App_FastChannel_ReportImuMetrics(
  const app_task_metrics_t *metrics
) {
  assert(metrics);

  Sys_Critical_Enter();
  s_imu_metrics = *metrics;
  Sys_Critical_Exit();
}
