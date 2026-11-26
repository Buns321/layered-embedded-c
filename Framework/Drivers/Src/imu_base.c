/**
 * @file imu_base.c
 * @brief 9 通道 IMU 多态基类与快照合并实现。
 * @ingroup drivers
 */

#include "imu_base.h"
#include "sys_angle.h"
#include "sys_critical.h"
#include "sys_math.h"
#include <stddef.h>
#include <string.h>

static float *imu_value_slot(imu_sample_t *sample, uint32_t channel) {
  switch (channel) {
  case IMU_CHANNEL_INDEX_YAW:
    return &sample->yaw_deg;
  case IMU_CHANNEL_INDEX_ROLL:
    return &sample->roll_deg;
  case IMU_CHANNEL_INDEX_PITCH:
    return &sample->pitch_deg;
  case IMU_CHANNEL_INDEX_GYRO_X:
    return &sample->gyro_dps[0];
  case IMU_CHANNEL_INDEX_GYRO_Y:
    return &sample->gyro_dps[1];
  case IMU_CHANNEL_INDEX_GYRO_Z:
    return &sample->gyro_dps[2];
  case IMU_CHANNEL_INDEX_ACCEL_X:
    return &sample->accel_mps2[0];
  case IMU_CHANNEL_INDEX_ACCEL_Y:
    return &sample->accel_mps2[1];
  case IMU_CHANNEL_INDEX_ACCEL_Z:
    return &sample->accel_mps2[2];
  default:
    return NULL;
  }
}

static bool imu_sanitize_frame(const filter_sample_t *input, filter_sample_t *sanitized) {
  if ((input->valid_mask & ~IMU_CHANNEL_MASK) != 0U) {
    return false;
  }

  *sanitized = *input;
  for (uint32_t ch = 0U; ch < IMU_CHANNEL_INDEX_COUNT; ++ch) {
    uint32_t mask = (uint32_t)1U << ch;
    if ((sanitized->valid_mask & mask) != 0U && !SYS_ISFINITE(sanitized->value[ch])) {
      sanitized->valid_mask &= ~mask;
    }
  }
  return sanitized->valid_mask != 0U;
}

static bool imu_merge_frame(imu_sample_t *sample, const filter_sample_t *frame, uint32_t clear_mask,
                            float yaw_offset_deg) {
  bool updated = false;

  for (uint32_t ch = 0U; ch < IMU_CHANNEL_INDEX_COUNT; ++ch) {
    uint32_t mask = (uint32_t)1U << ch;

    if ((clear_mask & mask) != 0U && (frame->valid_mask & mask) == 0U) {
      sample->valid_mask &= ~mask;
    }
    if ((frame->valid_mask & mask) == 0U) {
      continue;
    }

    float value = frame->value[ch];
    if (!SYS_ISFINITE(value)) {
      sample->valid_mask &= ~mask;
      continue;
    }

    if (ch == IMU_CHANNEL_INDEX_YAW) {
      float wrapped_deg = 0.0f;
      if (!Sys_Angle_WrapDegrees(value - yaw_offset_deg, &wrapped_deg)) {
        sample->valid_mask &= ~mask;
        continue;
      }
      value = wrapped_deg;
    }

    float *slot = imu_value_slot(sample, ch);
    if (slot == NULL) {
      continue;
    }
    *slot = value;
    sample->valid_mask |= mask;
    sample->channel_update_ms[ch] = frame->time_ms;
    sample->sample_time_ms = frame->time_ms;
    updated = true;
  }

  if (updated) {
    sample->sequence++;
  }
  return updated;
}

void Dev_IMU_Base_Init(imu_base_t *imu_h, const char *name, const imu_ops_t *ops,
                       filter_base_t *filter_h) {
  if (imu_h == NULL || name == NULL || ops == NULL || filter_h == NULL || ops->start == NULL ||
      ops->process == NULL || ops->get_status == NULL || ops->set_zero == NULL) {
    return;
  }
  (void)memset(imu_h, 0, sizeof(*imu_h));
  imu_h->ops = ops;
  imu_h->name = name;
  imu_h->filter_h = filter_h;
}

void Dev_IMU_Base_Start(imu_base_t *imu_h) {
  if (imu_h == NULL || imu_h->ops == NULL || imu_h->ops->start == NULL || imu_h->filter_h == NULL ||
      imu_h->is_started) {
    return;
  }
  Filter_Base_Reset(imu_h->filter_h);
  imu_h->ops->start(imu_h);
  imu_h->is_started = true;
}

void Dev_IMU_Base_Process(imu_base_t *imu_h, uint32_t now_ms) {
  if (imu_h == NULL || imu_h->ops == NULL || imu_h->ops->process == NULL || !imu_h->is_started) {
    return;
  }
  imu_h->ops->process(imu_h, now_ms);
}

bool Dev_IMU_Base_SubmitSample(imu_base_t *imu_h, const filter_sample_t *frame) {
  if (imu_h == NULL || frame == NULL || imu_h->filter_h == NULL || !imu_h->is_started) {
    return false;
  }

  filter_sample_t sanitized;
  if (!imu_sanitize_frame(frame, &sanitized)) {
    return false;
  }

  Sys_Critical_Enter();
  (void)imu_merge_frame(&imu_h->raw_sample, &sanitized, 0U, 0.0f);
  Sys_Critical_Exit();

  filter_sample_t filtered;
  Filter_Base_Update(imu_h->filter_h, &sanitized, &filtered);

  Sys_Critical_Enter();
  bool published =
      imu_merge_frame(&imu_h->sample, &filtered, sanitized.valid_mask, imu_h->yaw_offset_deg);
  if (published) {
    imu_h->sample_count++;
  }
  Sys_Critical_Exit();

  return published;
}

bool Dev_IMU_Base_GetSample(const imu_base_t *imu_h, imu_sample_t *sample) {
  if (imu_h == NULL || sample == NULL) {
    return false;
  }
  Sys_Critical_Enter();
  *sample = imu_h->sample;
  bool available = imu_h->is_started && (sample->sequence != 0U);
  Sys_Critical_Exit();
  return available;
}

bool Dev_IMU_Base_GetRawSample(const imu_base_t *imu_h, imu_sample_t *sample) {
  if (imu_h == NULL || sample == NULL) {
    return false;
  }
  Sys_Critical_Enter();
  *sample = imu_h->raw_sample;
  bool available = imu_h->is_started && (sample->sequence != 0U);
  Sys_Critical_Exit();
  return available;
}

void Dev_IMU_Base_GetStatus(imu_base_t *imu_h, imu_status_t *status) {
  if (imu_h == NULL || imu_h->ops == NULL || imu_h->ops->get_status == NULL || status == NULL) {
    return;
  }

  imu_status_t dev_status = {0};
  imu_h->ops->get_status(imu_h, &dev_status);

  Sys_Critical_Enter();
  dev_status.is_started = imu_h->is_started;
  dev_status.has_timestamp = (imu_h->sample.sequence != 0U);
  dev_status.valid_mask = imu_h->sample.valid_mask;
  dev_status.sample_count = imu_h->sample_count;
  dev_status.last_update_ms = imu_h->sample.sample_time_ms;
  (void)memcpy(dev_status.channel_update_ms, imu_h->sample.channel_update_ms,
               sizeof(dev_status.channel_update_ms));
  Sys_Critical_Exit();

  *status = dev_status;
}

float Dev_IMU_Base_GetYaw(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.yaw_deg;
}

float Dev_IMU_Base_GetRoll(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.roll_deg;
}

float Dev_IMU_Base_GetPitch(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.pitch_deg;
}

float Dev_IMU_Base_GetGyroX(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[0];
}

float Dev_IMU_Base_GetGyroY(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[1];
}

float Dev_IMU_Base_GetGyroZ(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[2];
}

float Dev_IMU_Base_GetAccelX(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[0];
}

float Dev_IMU_Base_GetAccelY(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[1];
}

float Dev_IMU_Base_GetAccelZ(const imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[2];
}

void Dev_IMU_Base_ZeroYaw(imu_base_t *imu_h) {
  if (imu_h == NULL) {
    return;
  }
  Sys_Critical_Enter();
  if ((imu_h->sample.valid_mask & IMU_CHANNEL_YAW) != 0U) {
    float wrapped_deg = 0.0f;
    if (Sys_Angle_WrapDegrees(imu_h->yaw_offset_deg + imu_h->sample.yaw_deg, &wrapped_deg)) {
      imu_h->yaw_offset_deg = wrapped_deg;
      imu_h->sample.yaw_deg = 0.0f;
    }
  }
  Sys_Critical_Exit();
}

void Dev_IMU_Base_SetZero(imu_base_t *imu_h) {
  if (imu_h == NULL || imu_h->ops == NULL || imu_h->ops->set_zero == NULL) {
    return;
  }
  imu_h->ops->set_zero(imu_h);
}
