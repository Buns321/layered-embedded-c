/**
 * @file telemetry_base.c
 * @brief 遥测设备多态基类实现。
 * @details 属于 Framework/Drivers 设备驱动层，负责虚表绑定、通道掩码越界检查、
 *          有效通道有限性校验（拦截 NaN/Inf）以及向具体协议后端转发。
 * @ingroup Framework_Drivers
 */

#include "telemetry_base.h"
#include "sys_math.h"
#include <stddef.h>

void Dev_Telemetry_Base_Init(telemetry_base_t *telemetry_h, const char *name,
                             const telemetry_ops_t *ops) {
  if (telemetry_h == NULL || name == NULL || ops == NULL || ops->send == NULL ||
      ops->get_status == NULL) {
    return;
  }
  telemetry_h->name = name;
  telemetry_h->ops = ops;
}

dev_telemetry_status_t Dev_Telemetry_Base_Send(telemetry_base_t *telemetry_h,
                                               const telemetry_sample_t *sample) {
  if (telemetry_h == NULL || telemetry_h->ops == NULL || telemetry_h->ops->send == NULL ||
      sample == NULL) {
    return DEV_TELEMETRY_ERROR;
  }
  if ((sample->valid_mask & ~TELEMETRY_CHANNEL_MASK) != 0U) {
    return DEV_TELEMETRY_ERROR;
  }

  // 仅检查 valid_mask 置位的通道是否为有限浮点数，避免未启用通道的垃圾值阻塞发送
  for (uint32_t ch = 0U; ch < TELEMETRY_CHANNEL_CAPACITY; ++ch) {
    uint32_t mask = (uint32_t)1U << ch;
    if ((sample->valid_mask & mask) != 0U && !SYS_ISFINITE(sample->value[ch])) {
      return DEV_TELEMETRY_ERROR;
    }
  }

  return telemetry_h->ops->send(telemetry_h, sample);
}

void Dev_Telemetry_Base_GetStatus(const telemetry_base_t *telemetry_h, telemetry_status_t *status) {
  if (telemetry_h == NULL || telemetry_h->ops == NULL || telemetry_h->ops->get_status == NULL ||
      status == NULL) {
    return;
  }
  telemetry_h->ops->get_status(telemetry_h, status);
}
