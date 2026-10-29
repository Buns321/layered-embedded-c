//
// 由 Buns_ 创建于 2026/9/1。
//

/**
 * @file imu_base.c
 * @brief IMU 抽象与统一滤波管线实现。
 * @ingroup drivers_base
 */

#include "imu_base.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 把角度归一化到 [-180, 180) 度。
 */
static float imu_wrap_deg(float angle_deg) {
  float wrapped = angle_deg;

  while (wrapped > 180.0f) {
    wrapped -= 360.0f;
  }
  while (wrapped < -180.0f) {
    wrapped += 360.0f;
  }

  return wrapped;
}

/**
 * @brief 根据通道位掩码定位样本中的浮点字段。
 */
static float *imu_value_slot(imu_sample_t *sample, uint32_t channel) {
  assert(sample && channel < IMU_CHANNEL_INDEX_COUNT);

  switch (channel) {
    case IMU_CHANNEL_INDEX_YAW:     return &sample->yaw_deg;
    case IMU_CHANNEL_INDEX_ROLL:    return &sample->roll_deg;
    case IMU_CHANNEL_INDEX_PITCH:   return &sample->pitch_deg;
    case IMU_CHANNEL_INDEX_GYRO_X:  return &sample->gyro_dps[0];
    case IMU_CHANNEL_INDEX_GYRO_Y:  return &sample->gyro_dps[1];
    case IMU_CHANNEL_INDEX_GYRO_Z:  return &sample->gyro_dps[2];
    case IMU_CHANNEL_INDEX_ACCEL_X: return &sample->accel_mps2[0];
    case IMU_CHANNEL_INDEX_ACCEL_Y: return &sample->accel_mps2[1];
    case IMU_CHANNEL_INDEX_ACCEL_Z: return &sample->accel_mps2[2];
    default:                        return NULL;
  }
}

/**
 * @brief 复制并清理输入帧中的非有限值。
 */
static bool imu_sanitize_frame(
  const filter_sample_t *input,
  filter_sample_t *sanitized
) {
  assert(input && sanitized);
  assert((input->valid_mask & ~IMU_CHANNEL_MASK) == 0u);

  *sanitized = *input;

  for (
    uint32_t channel = 0u;
    channel < IMU_CHANNEL_INDEX_COUNT;
    channel++
  ) {
    uint32_t mask = (uint32_t)1u << channel;
    if (
      (sanitized->valid_mask & mask) != 0u &&
      !SYS_ISFINITE(sanitized->value[channel])
    ) {
      sanitized->valid_mask &= ~mask;
    }
  }

  return sanitized->valid_mask != 0u;
}

/**
 * @brief 把校正后的滤波器帧合并回 IMU 样本。
 */
static bool imu_merge_frame(
  imu_sample_t *sample,
  const filter_sample_t *frame,
  uint32_t clear_mask,
  float yaw_offset_deg
) {
  assert(sample && frame);

  bool updated = false;

  for (
    uint32_t channel = 0u;
    channel < IMU_CHANNEL_INDEX_COUNT;
    channel++
  ) {
    uint32_t mask = (uint32_t)1u << channel;

    if (
      (clear_mask & mask) != 0u &&
      (frame->valid_mask & mask) == 0u
    ) {
      sample->valid_mask &= ~mask;
    }

    if ((frame->valid_mask & mask) == 0u) {
      continue;
    }

    float value = frame->value[channel];
    if (!SYS_ISFINITE(value)) {
      sample->valid_mask &= ~mask;
      continue;
    }

    if (channel == IMU_CHANNEL_INDEX_YAW) {
      value = imu_wrap_deg(value - yaw_offset_deg);
    }

    float *value_slot = imu_value_slot(sample, channel);
    assert(value_slot != NULL);

    *value_slot = value;
    sample->valid_mask |= mask;
    sample->channel_update_ms[channel] = frame->time_ms;
    sample->sample_time_ms = frame->time_ms;
    updated = true;
  }

  if (updated) {
    sample->sequence++;
  }

  return updated;
}

/**
 * @brief 初始化 IMU 抽象层。
 * @param imu_h 对象。
 * @param name 名称。
 * @param ops 具体实现操作表。
 * @param filter_h 已初始化的静态滤波器根节点，不能为空。
 */
void Dev_IMU_Base_Init(
  imu_base_t *imu_h,
  const char *name,
  const imu_ops_t *ops,
  filter_base_t *filter_h
) {
  assert(
    imu_h         &&
    name         &&
    ops          &&
    filter_h       &&
    ops->start   &&
    ops->process &&
    ops->get_status &&
    ops->set_zero
  );

  memset(imu_h, 0, sizeof(*imu_h));
  imu_h->ops    = ops;
  imu_h->name   = name;
  imu_h->filter_h = filter_h;
}

/** @brief 复位滤波器并启动具体 IMU。 */
void Dev_IMU_Base_Start(imu_base_t *imu_h) {
  assert(imu_h && imu_h->ops && imu_h->filter_h && !imu_h->is_started);

  Filter_Base_Reset(imu_h->filter_h);
  imu_h->ops->start(imu_h);
  imu_h->is_started = true;
}

/**
 * @brief 推进 IMU 的非阻塞状态机。
 * @param imu_h IMU 对象。
 * @param now_ms 当前毫秒时间戳。
 * @note 每个任务周期调用一次。
 */
void Dev_IMU_Base_Process(imu_base_t *imu_h, uint32_t now_ms) {
  assert(imu_h && imu_h->ops && imu_h->ops->process && imu_h->is_started);
  imu_h->ops->process(imu_h, now_ms);
}

/**
 * @brief 提交一帧由具体驱动采集的原始数据。
 * @param imu_h IMU 对象。
 * @param frame 原始滤波样本，只标记本帧更新的通道。
 * @return 滤波器是否产生了至少一个可发布通道。
 * @note 仅允许单写者调用；输入值必须是工程单位。
 * @note 调用方不能保持 frame；base 会完成滤波和结果合并。
 */
bool Dev_IMU_Base_SubmitSample(
  imu_base_t *imu_h,
  const filter_sample_t *frame
) {
  assert(
    imu_h           &&
    frame          &&
    imu_h->filter_h   &&
    imu_h->is_started
  );

  filter_sample_t sanitized;
  if (!imu_sanitize_frame(frame, &sanitized)) {
    return false;
  }

  Sys_Critical_Enter();
  (void)imu_merge_frame(&imu_h->raw_sample, &sanitized, 0u, 0.0f);
  Sys_Critical_Exit();

  filter_sample_t filtered;
  Filter_Base_Update(imu_h->filter_h, &sanitized, &filtered);

  Sys_Critical_Enter();

  bool published = imu_merge_frame(
    &imu_h->sample,
    &filtered,
    sanitized.valid_mask,
    imu_h->yaw_offset_deg
  );

  if (published) {
    imu_h->sample_count++;
  }

  Sys_Critical_Exit();

  return published;
}

/** @brief 复制一份经过滤波的 IMU 数据快照。 */
bool Dev_IMU_Base_GetSample(imu_base_t *imu_h, imu_sample_t *sample) {
  assert(imu_h && sample);

  Sys_Critical_Enter();
  *sample = imu_h->sample;
  bool available = imu_h->is_started && sample->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

/** @brief 复制一份未经软件滤波的 IMU 数据快照。 */
bool Dev_IMU_Base_GetRawSample(imu_base_t *imu_h, imu_sample_t *sample) {
  assert(imu_h && sample);

  Sys_Critical_Enter();
  *sample = imu_h->raw_sample;
  bool available = imu_h->is_started && sample->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

/** @brief 复制一份 IMU 运行状态快照。 */
void Dev_IMU_Base_GetStatus(imu_base_t *imu_h, imu_status_t *status) {
  assert(imu_h && imu_h->ops && imu_h->ops->get_status && status);

  imu_status_t device_status = {0};
  imu_h->ops->get_status(imu_h, &device_status);

  Sys_Critical_Enter();
  device_status.is_started = imu_h->is_started;
  device_status.has_timestamp = imu_h->sample.sequence != 0u;
  device_status.valid_mask = imu_h->sample.valid_mask;
  device_status.sample_count = imu_h->sample_count;
  device_status.last_update_ms = imu_h->sample.sample_time_ms;

  memcpy(
    device_status.channel_update_ms,
    imu_h->sample.channel_update_ms,
    sizeof(device_status.channel_update_ms)
  );

  Sys_Critical_Exit();

  *status = device_status;
}

/** @brief 读取滤波后的 yaw（度）。 */
float Dev_IMU_Base_GetYaw(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.yaw_deg;
}

/** @brief 读取滤波后的 roll（度）。 */
float Dev_IMU_Base_GetRoll(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.roll_deg;
}

/** @brief 读取滤波后的 pitch（度）。 */
float Dev_IMU_Base_GetPitch(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.pitch_deg;
}

/** @brief 读取滤波后的 X 轴角速度（deg/s）。 */
float Dev_IMU_Base_GetGyroX(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[0];
}

/** @brief 读取滤波后的 Y 轴角速度（deg/s）。 */
float Dev_IMU_Base_GetGyroY(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[1];
}

/** @brief 读取滤波后的 Z 轴角速度（deg/s）。 */
float Dev_IMU_Base_GetGyroZ(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[2];
}

/** @brief 读取滤波后的 X 轴加速度（m/s^2）。 */
float Dev_IMU_Base_GetAccelX(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[0];
}

/** @brief 读取滤波后的 Y 轴加速度（m/s^2）。 */
float Dev_IMU_Base_GetAccelY(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[1];
}

/** @brief 读取滤波后的 Z 轴加速度（m/s^2）。 */
float Dev_IMU_Base_GetAccelZ(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[2];
}

/**
 * @brief 把当前滤波后的 yaw 设为零参考，不改变 raw_sample。
 * @note 该操作只影响对外发布值，不修改滤波器内部状态。
 */
void Dev_IMU_Base_ZeroYaw(imu_base_t *imu_h) {
  assert(imu_h);

  Sys_Critical_Enter();

  if ((imu_h->sample.valid_mask & IMU_CHANNEL_YAW) != 0u) {
    imu_h->yaw_offset_deg = imu_wrap_deg(
      imu_h->yaw_offset_deg + imu_h->sample.yaw_deg
    );
    imu_h->sample.yaw_deg = 0.0f;
  }

  Sys_Critical_Exit();
}

/** @brief 请求具体 IMU 执行设备侧置零动作。 */
void Dev_IMU_Base_SetZero(imu_base_t *imu_h) {
  assert(imu_h && imu_h->ops && imu_h->ops->set_zero);
  imu_h->ops->set_zero(imu_h);
}
