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

/**
 * @brief 不区分 ASCII 大小写比较两个文本是否完全相等。
 * @param left 左侧 NUL 结尾文本，必须非 NULL，允许空字符串，调试构建以 assert 检查。
 * @param right 右侧 NUL 结尾文本，必须非 NULL，允许空字符串，调试构建以 assert 检查。
 * @retval true 两个文本长度相同，且 ASCII 字母忽略大小写后逐字符相等。
 * @retval false 任一字符不同或长度不同。
 * @pre left 和 right 非 NULL，并保持有效直到函数返回。
 * @note 只读纯函数；不分配内存，仅折叠 ASCII 字母大小写。
 */
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

/**
 * @brief 按有效电平配置驱动蜂鸣器 GPIO。
 * @param enabled true 表示请求蜂鸣器发声，false 表示关闭；有效电平由 APP_BUZZER_ON_LEVEL 决定。
 * @post APP_BUZZER_PORT/PIN 的物理输出与请求状态一致；不修改蜂鸣器相位状态机。
 * @note 在 ImuEkfTask 或初始化上下文调用；GPIO 写入不阻塞，不分配内存。
 */
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

/**
 * @brief 按有效电平配置驱动 LED GPIO。
 * @param enabled true 表示请求 LED 点亮，false 表示熄灭；有效电平由 APP_LED_ON_LEVEL 决定。
 * @post APP_LED_PORT/PIN 的物理输出与请求状态一致；不修改 led_output_on 逻辑状态。
 * @note 在 ImuEkfTask 或初始化上下文调用；GPIO 写入不阻塞，不分配内存。
 */
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

/**
 * @brief 启动双短鸣提示并记录当前时间。
 * @param now_ms 当前单调毫秒时间戳，作为第一段短鸣的相位起点。
 * @post 蜂鸣器立即开启，相位置为 SHORT_ON_1，后续由 app_fastchannel_processbuzzer 推进。
 * @note 若已有鸣叫序列正在执行，本函数会以新序列覆盖它；不阻塞、不分配内存。
 */
static void app_fastchannel_startshortbuzzer(uint32_t now_ms) {
  s_app.buzzer.phase = APP_BUZZER_PHASE_SHORT_ON_1;
  s_app.buzzer.phase_started_ms = now_ms;
  app_fastchannel_writebuzzer(true);
}

/**
 * @brief 启动长鸣提示并记录当前时间。
 * @param now_ms 当前单调毫秒时间戳，作为长鸣相位起点。
 * @post 蜂鸣器立即开启，相位置为 LONG_ON，600 ms 后由处理函数关闭。
 * @note 若已有鸣叫序列正在执行，本函数会以长鸣覆盖它；不阻塞、不分配内存。
 */
static void app_fastchannel_startlongbuzzer(uint32_t now_ms) {
  s_app.buzzer.phase = APP_BUZZER_PHASE_LONG_ON;
  s_app.buzzer.phase_started_ms = now_ms;
  app_fastchannel_writebuzzer(true);
}

/**
 * @brief 推进蜂鸣器时序状态机并关闭到期的输出。
 * @param now_ms 当前单调毫秒时间戳；必须与启动相位时使用同一时基。
 * @post 根据 80/120/80/600 ms 时序推进双短鸣或长鸣，并在 OFF 相位持续保持蜂鸣器关闭。
 * @note 允许调用间隔大于单段时长，单次调用最多推进一个相位；不阻塞、不分配内存。
 */
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

/**
 * @brief 按运行状态切换 LED 闪烁周期。
 * @param now_ms 当前单调毫秒时间戳，用于判断半周期是否到达。
 * @post 主运行状态每 500 ms 翻转一次，其他状态每 250 ms 翻转一次，并立即更新 LED 引脚。
 * @note 仅由 ImuEkfTask 调用；不阻塞、不分配内存。
 */
static void app_fastchannel_processled(uint32_t now_ms) {
  uint32_t half_period_ms =
    s_app.calibration_state == APP_FAST_CHANNEL_STATE_MAIN ? 500u : 250u;

  if (now_ms - s_app.last_led_toggle_ms >= half_period_ms) {
    s_app.last_led_toggle_ms = now_ms;
    s_app.led_output_on = !s_app.led_output_on;
    app_fastchannel_writeled(s_app.led_output_on);
  }
}

/**
 * @brief 构造全零且比例因子为 1 的默认校准记录。
 * @param record 输出记录，必须可写且非 NULL，所有权仍归调用方。
 * @pre record 非 NULL，调试构建以 assert 检查。
 * @post face_mask/completed_axis_mask 和全部零偏清零，三轴 accel_scale 均为 1.0。
 * @note 不分配内存；用于 Flash 记录缺失或损坏时的安全回退。
 */
static void app_fastchannel_defaultrecord(
  app_calibration_record_t *record
) {
  assert(record);
  memset(record, 0, sizeof(*record));
  for (uint8_t axis = 0u; axis < APP_AXIS_COUNT; axis++) {
    record->accel_scale[axis] = 1.0f;
  }
}

/**
 * @brief 把校准记录中的角度单位转换后应用到 IMU 校正滤波器。
 * @param record 只读校准记录；零偏单位 rad/s，加速度零偏 m/s²，比例系数无量纲。
 * @pre record 非 NULL，内容已由加载校验或标定流程验证，调试构建以 assert 检查。
 * @post 加速度零偏/比例原样下发，陀螺零偏换算为 deg/s 后下发到 s_calibration_filter。
 * @warning 底层配置结果被忽略；调用方必须只传入已验证记录，否则滤波器可能保留旧配置。
 * @note 不分配内存；仅由 ImuEkfTask 或初始化路径调用。
 */
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

/**
 * @brief 从记录存储读取并校验最新校准记录。
 * @retval true 最新记录版本、负载长度、掩码、有限值和比例系数范围均合法。
 * @retval false 存储为空/损坏、版本或长度不匹配、掩码越界，或任一标定值非法。
 * @post 成功时 s_app.active_record 被记录内容覆盖；失败时保持默认或已有活动记录不变。
 * @pre Sys_RecordStore_Init 已成功，且调用期间独占 Flash 记录存储；本函数只在初始化阶段调用。
 * @note 不分配内存；失败时不覆盖 active_record，调用方继续使用默认记录。
 */
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

/**
 * @brief 持久化工作校准记录并更新活动配置。
 * @retval true 记录已追加到 Flash，active_record 已更新并应用到校正滤波器。
 * @retval false Flash 追加失败；flash_write_failed 置 true，active_record 和滤波器保持旧值。
 * @post 成功时更新活动记录、清空 flash_write_failed 并设置 flash_record_valid。
 * @note 仅在 ImuEkfTask 标定流程调用；包含 Flash 擦写等待，不得在 ISR 调用，不分配内存。
 */
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

/**
 * @brief 使用原始校准阈值判断 IMU 样本是否静止。
 * @param sample 只读 IMU 样本，加速度单位 m/s²、角速度单位 deg/s。
 * @retval true 加速度模长在 7.8..11.8 m/s²，且三轴原始角速度绝对值不超过 10 deg/s。
 * @retval false 任一分量非有限，或超出原始静止门限。
 * @pre sample 非 NULL，调试构建以 assert 检查。
 * @note 使用未校正陀螺阈值，适用于零偏偏移尚未标定的初始阶段；纯只读，不分配内存。
 */
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

/**
 * @brief 检测样本对应的安装面，失败时返回 0。
 * @param sample 只读 IMU 样本；使用其原始三轴加速度（m/s²）和原始面识别门限。
 * @return 成功时返回 APP_FACE_X/Y/Z_POSITIVE/NEGATIVE 之一；倾斜或非有限时返回 0。
 * @pre sample 非 NULL，调试构建以 assert 检查。
 * @note 纯只读函数；不更新面掩码，不分配内存。
 */
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

/**
 * @brief 把安装面位掩码映射为记录下标。
 * @param face 单个安装面位：APP_FACE_X_POSITIVE 至 APP_FACE_Z_NEGATIVE。
 * @return X+..Z- 依次映射为 0..5；不是单个合法面时返回 UINT8_MAX。
 * @note 纯函数；调用方必须区分合法下标和 UINT8_MAX，不分配内存。
 */
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

/**
 * @brief 把安装面映射为对应传感器轴。
 * @param face 单个安装面位；X/Y 正负映射到 0/1，其他值映射到 2。
 * @return X 面返回 0，Y 面返回 1，Z 面及非法输入返回 2。
 * @warning 本函数不校验 face；调用方须先用 app_fastchannel_faceindex 验证，否则非法值会被当作 Z 轴。
 * @note 纯函数；不分配内存，不修改面状态。
 */
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

/**
 * @brief 校验并累计当前安装面的校准样本。
 * @param face 待接受的单个安装面位；必须属于 APP_FACE_* 六个合法值之一。
 * @param accel_mean 该面的三轴平均加速度，按 X/Y/Z 排列，单位 m/s²；容量至少 3 个 float。
 * @retval true 面已写入 working_record；若已有相对面，则同时完成该轴零偏和比例计算。
 * @retval false face 非法、相对样本跨度不足，或计算出的比例系数越界；标定状态不变。
 * @pre 仅在 SAMPLE_FACE 完成 100 点平均后调用；accel_mean 非 NULL，调试构建以 assert 检查。
 * @post 成功时把三轴均值写入 face_values，face_mask 增加该面并更新对应轴记录；本函数不写 Flash。
 * @note 仅 ImuEkfTask 调用；不分配内存。
 */
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

/**
 * @brief 进入陀螺零偏校准状态并清空累计器。
 * @param now_ms 当前单调毫秒时间戳，作为 30 秒校准窗口起点。
 * @post calibration_state 置为 GYRO_CALIBRATION，采样计数清零，三轴和与平方和清零。
 * @note 仅由 ImuEkfTask 调用；不写 Flash、不分配内存。
 */
static void app_fastchannel_entergyrocalibration(uint32_t now_ms) {
  s_app.calibration_state = APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION;
  s_app.state_started_ms = now_ms;
  s_app.gyro_sample_count = 0u;
  memset(s_app.gyro_sum_dps, 0, sizeof(s_app.gyro_sum_dps));
  memset(s_app.gyro_sum_sq_dps, 0, sizeof(s_app.gyro_sum_sq_dps));
}

/**
 * @brief 切换到主运行状态并刷新状态时间。
 * @param now_ms 当前单调毫秒时间戳，作为 MAIN 状态起点。
 * @param reason 状态切换原因文本，NUL 结尾且必须非 NULL，允许空字符串；用于 RTT 日志，函数不保留指针。
 * @pre reason 非 NULL，生命周期至少覆盖本次调用。
 * @post 若尚未进入 MAIN：恢复 Flash 活动记录、清除未提交面进度、重置 EKF，并置 MAIN；
 *       首次进入时启动一次长鸣。重复调用不重复重置或鸣叫。
 * @note 仅由 ImuEkfTask 状态机调用；不分配内存。
 */
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

/**
 * @brief 处理初始姿态确认阶段。
 * @param now_ms 当前单调毫秒时间戳，用于 3 秒初始观察和倒置计时。
 * @param raw_sample 只读原始样本；要求三轴加速度 m/s²、角速度 deg/s 且相关通道有效。
 * @pre raw_sample 非 NULL，调试构建以 assert 检查。
 * @post 若静止且 az <= -7.8 m/s² 持续 3 秒，跳过六面流程直接进入 MAIN；
 *       否则初始观察满 3 秒后进入陀螺零偏校准。
 * @note 仅由 ImuEkfTask 调用；不分配内存，不直接写 Flash。
 */
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

/**
 * @brief 处理六面陀螺零偏校准流程。
 * @param now_ms 当前单调毫秒时间戳，用于 30 秒超时和累计采样。
 * @param raw_sample 只读原始样本；使用三轴角速度（deg/s）和静止判定所需加速度（m/s²）。
 * @pre raw_sample 非 NULL，调试构建以 assert 检查。
 * @post 连续静止累计 100 点且三轴标准差不超过 3 deg/s 时写入工作记录零偏并进入 WAIT_FACE。
 * @post 移动或标准差超差时清零累计并留在 GYRO_CALIBRATION；30 s 超时则进入 MAIN。
 * @note 仅在 ImuEkfTask 中调用；此阶段不提交 Flash，不分配内存。
 */
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

/**
 * @brief 判断当前安装面的采样是否超时。
 * @param now_ms 当前单调毫秒时间戳，使用与 face_deadline_ms 相同的回绕计数时基。
 * @retval true 有符号差 now_ms - face_deadline_ms >= 0，即窗口到期。
 * @retval false 尚未到达截止时间。
 * @note 纯判定函数；要求截止时间与 now_ms 的跨度小于 2^31 ms，不修改状态、不分配内存。
 */
static bool app_fastchannel_facetimedout(uint32_t now_ms) {
  return (int32_t)(now_ms - s_app.face_deadline_ms) >= 0;
}

/**
 * @brief 清空安装面搜索状态和计时。
 * @param now_ms 当前单调毫秒时间戳，作为下一轮 30 秒面搜索窗口起点。
 * @post 状态回到 WAIT_FACE，候选面、单面采样计数清零，face_deadline_ms 设为 now_ms + 30 秒。
 * @note 已接受的面和轴完成掩码保持不变；不分配内存，不写 Flash。
 */
static void app_fastchannel_resetfacesearch(uint32_t now_ms) {
  s_app.calibration_state = APP_FAST_CHANNEL_STATE_WAIT_FACE;
  s_app.state_started_ms = now_ms;
  s_app.face_sample_count = 0u;
  s_app.candidate_face = 0u;
  s_app.face_deadline_ms =
    now_ms + APP_CALIBRATION_FACE_TIMEOUT_MS;
}

/**
 * @brief 推进安装面搜索状态机。
 * @param now_ms 当前单调毫秒时间戳，用于稳定 1 秒、单面 30 秒窗口和蜂鸣器相位。
 * @param raw_sample 只读原始样本，用于面识别和三轴加速度（m/s²）平均。
 * @param calibrated_sample 只读校正后样本，仅使用其角速度（deg/s）判断静止。
 * @pre raw_sample 和 calibrated_sample 非 NULL，调试构建以 assert 检查。
 * @post WAIT_FACE 识别新面后进入 1 秒 STABILIZE_FACE；稳定后采集 100 点进入 SAMPLE_FACE。
 * @post 每面提交后尝试写 Flash；非最终面写入成功时短鸣并继续搜索，最终面写入成功时由 MAIN 入口长鸣。
 * @post Flash 写失败会置 flash_write_failed 且不短鸣；非最终面保留内存进度继续搜索，最终面仍进入 MAIN，但回退到最近成功持久化的记录。
 * @post 面偏移、样本跨度非法或超时会丢弃未提交进度并回到安全状态，不使能任何执行器。
 * @note 仅由 ImuEkfTask 调用；状态机内可执行 Flash 写入，不分配内存。
 */
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

/**
 * @brief 把角度归一化到 [-pi, pi] 区间。
 * @param angle_rad 待归一化角度，单位 rad；实际有效输出范围为 [-pi, pi]。
 * @return 与输入相差 2*pi 整数倍且落在 [-pi, pi] 的角度，单位 rad。
 * @warning 输入必须是有限值；+inf/-inf 会使 while 循环永不结束。正常调用来自有限陀螺积分。
 * @note 纯函数；不分配内存，不修改共享状态。
 */
static float app_fastchannel_wrapradians(float angle_rad) {
  while (angle_rad > APP_PI) {
    angle_rad -= 2.0f * APP_PI;
  }
  while (angle_rad < -APP_PI) {
    angle_rad += 2.0f * APP_PI;
  }
  return angle_rad;
}

/**
 * @brief 更新直接姿态角、陀螺积分角和实际采样周期。
 * @param now_ms 当前单调毫秒时间戳，用于计算并限幅实际 dt_s。
 * @param sample 只读校正后样本；加速度 m/s²，角速度 deg/s，有效性由 valid_mask 标记。
 * @pre sample 非 NULL，调试构建以 assert 检查。
 * @post 加速度 Z 有效时更新直接横滚/俯仰角；三轴陀螺均有效时按 dt_s 积分并归一化三轴角度。
 * @post dt_s 限制在 APP_EKF_MIN_DT_S..APP_EKF_MAX_DT_S；缺失通道对应的参考量保持不变。
 * @note 首次调用沿用初始化阶段的标称 dt_s；本函数不判断静止门限，收到有效校正样本即更新。
 * @note 仅由 ImuEkfTask 调用，不分配内存。
 */
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

/**
 * @brief 生成并发布当前快速通道遥测快照。
 * @param now_ms 当前单调毫秒时间戳；当前仅保留，未写入快照。
 * @param raw_valid true 表示尝试复制最新原始 MPU6050 样本，false 表示标记原始数据无效。
 * @param calibrated_valid true 表示尝试复制最新校正样本，false 表示标记校正数据无效。
 * @post 先完整写入非活动双缓冲，再在临界区内一次性切换 s_active_snapshot，保证读者看到一致快照。
 * @post 快照序号递增；EKF、标定状态、面掩码、Flash 状态和命令结果始终发布。
 * @note 仅由 ImuEkfTask 调用；不阻塞，不分配内存，临界区仅覆盖索引切换。
 */
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

/**
 * @brief 解析并应用主机命令的状态变更。
 * @param command 已解析命令，只读；SET 名称大小写不敏感，value 由解析器保证有限。
 * @param context 链路回调上下文；当前未使用，可为 NULL。
 * @retval true RESET EKF 已执行，或 SET 参数名受支持且数值通过 EKF 范围校验。
 * @retval false 命令类型、参数名未知，或参数值被 EKF 拒绝；EKF 配置保持不变。
 * @pre command 非 NULL，调试构建以 assert 检查；仅由 VofaTask 命令回调调用。
 * @post RESET 提交编译期 EKF 默认参数；SET 提交单个运行时参数，均在下一帧开始时生效，不写 Flash。
 * @note 与 ImuEkfTask 共享 EKF；Filter_Ekf_* 内部用短临界区交换待提交配置，本函数不额外加锁、不分配内存。
 */
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

/**
 * @brief 初始化板级设备、滤波链、Flash 校准和 UART 链路。
 * @retval PLATFORM_STATUS_OK 设备、滤波链、校准状态和命令链路已初始化；重复调用直接返回 OK。
 * @retval PLATFORM_STATUS_BUSY 物理 UART 回调槽已被其他所有者占用。
 * @retval PLATFORM_STATUS_ERROR IMU 校准滤波器、EKF、MPU6050 或 UART 端口初始化失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 板级静态配置或 UART 实例不满足底层前置条件。
 * @pre CubeMX 已初始化 I2C1、USART1、DMA1_Channel4、GPIO 和 CRC。
 * @pre 首次初始化必须在 RTOS 调度器启动前完成，并独占 I2C1、USART1 和 Flash；已初始化后的重复调用直接返回 OK。
 * @post 成功时进入 INITIAL_POSE_CHECK，电机外设尚未参与；RX 启动失败会记录警告并延后重试。
 * @post Flash 记录缺失或损坏不阻止启动，但 flash_record_valid 为 false；后续提交会尝试重建记录。
 * @note 不分配动态内存；初始化包含同步 I2C/Flash 操作，失败时模块保持未初始化。
 */
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

/**
 * @brief 执行一次 100 Hz IMU、标定和 EKF 处理。
 * @param now_ms 当前单调毫秒时间戳；首帧可传 0，state_started_ms 为 0 时会用该值初始化状态起点。
 * @pre 只允许 ImuEkfTask 在 10 ms 周期上下文中调用，且 App_FastChannel_Init 已成功返回。
 * @post 已驱动一次 MPU6050 采样/滤波，推进姿态、陀螺和六面标定状态机，并发布一致快照。
 * @post 同步推进蜂鸣器和 LED 时序；未初始化时函数直接返回，状态保持不变。
 * @note 任务上下文执行，可能等待最多 APP_IMU_I2C_TIMEOUT_MS 的 I2C 事务；六面提交时还可能在任务内执行 Flash 擦写并显著延长本次调用；不分配内存。
 */
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

/**
 * @brief 根据数据有效性更新遥测通道掩码。
 * @param sample 待修改的遥测样本，必须非 NULL 且可写。
 * @param channel 通道下标，有效范围 0..TELEMETRY_CHANNEL_CAPACITY-1。
 * @param valid true 置位该通道有效位，false 清除该通道有效位。
 * @pre sample 非 NULL 且 channel 在容量内，调试构建以 assert 检查。
 * @post valid_mask 仅修改 channel 对应的一位，其他通道值和掩码保持不变。
 * @note 纯本地掩码操作；不分配内存，不访问硬件。
 */
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

/**
 * @brief 处理命令并发送 50 Hz 32 通道 JustFloat。
 * @param now_ms 当前单调毫秒时间戳，同时写入遥测样本时间戳。
 * @pre 只允许 VofaTask 在 20 ms 周期上下文中调用，且 App_FastChannel_Init 已成功返回。
 * @post 已消费 RX 命令、复制最新一致快照、组装 32 通道 JustFloat 并发起一次非阻塞发送。
 * @post 约每 1000 ms 输出一条 RTT 状态日志；发送忙时整帧丢弃，不影响 IMU 状态机。
 * @note 任务上下文；命令回调同步执行，不分配内存，不等待 DMA 完成。
 */
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

/**
 * @brief 复制最近一次一致快照。
 * @param snapshot 输出快照；函数完整覆盖其全部字段，所有权仍归调用方。
 * @pre snapshot 非 NULL，可写且不与内部双缓冲重叠。
 * @post 返回的 raw/calibrated/EKF、标定状态、序列号和 Flash 标志来自同一个已发布缓冲区。
 * @note 使用短临界区复制整个快照；不分配内存，可从任务上下文调用。
 * @warning 复制期间会短暂关闭中断，禁止在 ISR 或高实时性路径调用。
 */
void App_FastChannel_GetSnapshot(
  app_fast_channel_snapshot_t *snapshot
) {
  assert(snapshot);

  Sys_Critical_Enter();
  *snapshot = s_snapshots[s_active_snapshot];
  Sys_Critical_Exit();
}

/**
 * @brief 更新 ImuEkfTask 性能统计。
 * @param metrics 最新执行时间、周期抖动和剩余栈字节；只读，调用方保留所有权。
 * @pre 只允许 ImuEkfTask 调用，metrics 非 NULL 且包含本单位换算后的有效统计。
 * @post 内部统计已在临界区内整体替换，供 VofaTask 日志读取；不改变姿态或标定状态。
 * @note 使用短 PRIMASK 临界区保证写侧一致；读取侧未加锁，三个字段可能来自相邻更新。
 * @note 不分配内存，调试构建对空指针 assert。
 */
void App_FastChannel_ReportImuMetrics(
  const app_task_metrics_t *metrics
) {
  assert(metrics);

  Sys_Critical_Enter();
  s_imu_metrics = *metrics;
  Sys_Critical_Exit();
}
