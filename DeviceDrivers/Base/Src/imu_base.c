//
// Created by Buns_ on 2026/9/1.
//

#include "imu_base.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

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

void Dev_IMU_Base_Init(
  imu_base_t *this,
  const char *name,
  const imu_ops_t *ops,
  filter_base_t *filter
) {
  assert(
    this         &&
    name         &&
    ops          &&
    filter       &&
    ops->start   &&
    ops->process &&
    ops->get_status &&
    ops->set_zero
  );

  memset(this, 0, sizeof(*this));
  this->ops    = ops;
  this->name   = name;
  this->filter = filter;
}

void Dev_IMU_Base_Start(imu_base_t *this) {
  assert(this && this->ops && this->filter && !this->is_started);

  Filter_Base_Reset(this->filter);
  this->ops->start(this);
  this->is_started = true;
}

void Dev_IMU_Base_Process(imu_base_t *this, uint32_t now_ms) {
  assert(this && this->ops && this->ops->process && this->is_started);
  this->ops->process(this, now_ms);
}

bool Dev_IMU_Base_SubmitSample(
  imu_base_t *this,
  const filter_sample_t *frame
) {
  assert(
    this           &&
    frame          &&
    this->filter   &&
    this->is_started
  );

  filter_sample_t sanitized;
  if (!imu_sanitize_frame(frame, &sanitized)) {
    return false;
  }

  Sys_Critical_Enter();
  (void)imu_merge_frame(&this->raw_sample, &sanitized, 0u, 0.0f);
  Sys_Critical_Exit();

  filter_sample_t filtered;
  Filter_Base_Update(this->filter, &sanitized, &filtered);

  Sys_Critical_Enter();

  bool published = imu_merge_frame(
    &this->sample,
    &filtered,
    sanitized.valid_mask,
    this->yaw_offset_deg
  );

  if (published) {
    this->sample_count++;
  }

  Sys_Critical_Exit();

  return published;
}

bool Dev_IMU_Base_GetSample(imu_base_t *this, imu_sample_t *sample) {
  assert(this && sample);

  Sys_Critical_Enter();
  *sample = this->sample;
  bool available = this->is_started && sample->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

bool Dev_IMU_Base_GetRawSample(imu_base_t *this, imu_sample_t *sample) {
  assert(this && sample);

  Sys_Critical_Enter();
  *sample = this->raw_sample;
  bool available = this->is_started && sample->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

void Dev_IMU_Base_GetStatus(imu_base_t *this, imu_status_t *status) {
  assert(this && this->ops && this->ops->get_status && status);

  imu_status_t device_status = {0};
  this->ops->get_status(this, &device_status);

  Sys_Critical_Enter();
  device_status.is_started = this->is_started;
  device_status.has_timestamp = this->sample.sequence != 0u;
  device_status.valid_mask = this->sample.valid_mask;
  device_status.sample_count = this->sample_count;
  device_status.last_update_ms = this->sample.sample_time_ms;

  memcpy(
    device_status.channel_update_ms,
    this->sample.channel_update_ms,
    sizeof(device_status.channel_update_ms)
  );

  Sys_Critical_Exit();

  *status = device_status;
}

float Dev_IMU_Base_GetYaw(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.yaw_deg;
}

float Dev_IMU_Base_GetRoll(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.roll_deg;
}

float Dev_IMU_Base_GetPitch(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.pitch_deg;
}

float Dev_IMU_Base_GetGyroX(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.gyro_dps[0];
}

float Dev_IMU_Base_GetGyroY(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.gyro_dps[1];
}

float Dev_IMU_Base_GetGyroZ(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.gyro_dps[2];
}

float Dev_IMU_Base_GetAccelX(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.accel_mps2[0];
}

float Dev_IMU_Base_GetAccelY(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.accel_mps2[1];
}

float Dev_IMU_Base_GetAccelZ(imu_base_t *this) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(this, &sample);
  return sample.accel_mps2[2];
}

void Dev_IMU_Base_ZeroYaw(imu_base_t *this) {
  assert(this);

  Sys_Critical_Enter();

  if ((this->sample.valid_mask & IMU_CHANNEL_YAW) != 0u) {
    this->yaw_offset_deg = imu_wrap_deg(
      this->yaw_offset_deg + this->sample.yaw_deg
    );
    this->sample.yaw_deg = 0.0f;
  }

  Sys_Critical_Exit();
}

void Dev_IMU_Base_SetZero(imu_base_t *this) {
  assert(this && this->ops && this->ops->set_zero);
  this->ops->set_zero(this);
}
