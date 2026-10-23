/**
 * @file platform_can_backend.h
 * @brief Private STM32-family backend for the common CAN implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H

#include "platform_def.h"
#include "platform_can.h"
#include <stdbool.h>

bool platform_can_backend_is_valid(platform_can_instance_t can);

platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
);

platform_status_t platform_can_backend_start(
  platform_can_instance_t can
);

platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
);

platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
);

platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
);

platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
);

platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
);

uint32_t platform_can_backend_get_error(platform_can_instance_t can);

void platform_can_dispatch_event(
  platform_can_instance_t can,
  const platform_can_event_t *event
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H
