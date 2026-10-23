/**
 * @file platform_can_backend.h
 * @brief Private STM32-family backend for the common CAN implementation.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H

#include "platform_def.h"
#include "platform_can.h"
#include <stdbool.h>

bool platform_can_backend_is_valid(Platform_CAN_Instance can);

Platform_StatusTypeDef platform_can_backend_configure_filter(
  Platform_CAN_Instance can,
  const Platform_CAN_FilterConfig *config
);

Platform_StatusTypeDef platform_can_backend_start(
  Platform_CAN_Instance can
);

Platform_StatusTypeDef platform_can_backend_stop(
  Platform_CAN_Instance can
);

Platform_StatusTypeDef platform_can_backend_transmit(
  Platform_CAN_Instance can,
  const Platform_CAN_Frame *frame,
  uint32_t timeout
);

Platform_StatusTypeDef platform_can_backend_receive(
  Platform_CAN_Instance can,
  Platform_CAN_FIFO fifo,
  Platform_CAN_Frame *frame,
  uint32_t timeout
);

Platform_StatusTypeDef platform_can_backend_activate_notification(
  Platform_CAN_Instance can,
  uint32_t notifications
);

Platform_StatusTypeDef platform_can_backend_deactivate_notification(
  Platform_CAN_Instance can,
  uint32_t notifications
);

uint32_t platform_can_backend_get_error(Platform_CAN_Instance can);

void platform_can_dispatch_event(
  Platform_CAN_Instance can,
  const Platform_CAN_Event *event
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_BACKEND_H
