//
// 由 Buns_ 创建于 2026/10/21。
//

#include "telemetry_base.h"
#include "system_math.h"
#include <assert.h>

void Dev_Telemetry_Base_Init(
  telemetry_base_t *this,
  const char *name,
  const telemetry_ops_t *ops
) {
  assert(
    this           &&
    name           &&
    ops            &&
    ops->send      &&
    ops->get_status
  );

  this->name = name;
  this->ops  = ops;
}

Dev_Telemetry_Status Dev_Telemetry_Base_Send(
  telemetry_base_t *this,
  const telemetry_sample_t *sample
) {
  assert(
    this              &&
    this->ops         &&
    this->ops->send   &&
    sample
  );

  if (
    this == NULL       ||
    this->ops == NULL  ||
    this->ops->send == NULL ||
    sample == NULL
  ) {
    return DEV_TELEMETRY_ERROR;
  }

  if ((sample->valid_mask & ~TELEMETRY_CHANNEL_MASK) != 0u) {
    return DEV_TELEMETRY_ERROR;
  }

  for (
    uint32_t channel = 0u;
    channel < TELEMETRY_CHANNEL_CAPACITY;
    channel++
  ) {
    uint32_t mask = (uint32_t)1u << channel;
    if (
      (sample->valid_mask & mask) != 0u &&
      !SYS_ISFINITE(sample->value[channel])
    ) {
      return DEV_TELEMETRY_ERROR;
    }
  }

  return this->ops->send(this, sample);
}

void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *this,
  telemetry_status_t *status
) {
  assert(
    this                    &&
    this->ops               &&
    this->ops->get_status   &&
    status
  );

  this->ops->get_status(this, status);
}
