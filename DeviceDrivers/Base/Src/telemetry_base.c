//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file telemetry_base.c
 * @brief 遥测设备抽象接口实现。
 * @ingroup drivers_base
 */

#include "telemetry_base.h"
#include "system_math.h"
#include <assert.h>

void Dev_Telemetry_Base_Init(
  telemetry_base_t *telemetry_h,
  const char *name,
  const telemetry_ops_t *ops
) {
  assert(
    telemetry_h           &&
    name           &&
    ops            &&
    ops->send      &&
    ops->get_status
  );

  telemetry_h->name = name;
  telemetry_h->ops  = ops;
}

dev_telemetry_status_t Dev_Telemetry_Base_Send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  assert(
    telemetry_h              &&
    telemetry_h->ops         &&
    telemetry_h->ops->send   &&
    sample
  );

  if (
    telemetry_h == NULL       ||
    telemetry_h->ops == NULL  ||
    telemetry_h->ops->send == NULL ||
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

  return telemetry_h->ops->send(telemetry_h, sample);
}

void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  assert(
    telemetry_h                    &&
    telemetry_h->ops               &&
    telemetry_h->ops->get_status   &&
    status
  );

  telemetry_h->ops->get_status(telemetry_h, status);
}
