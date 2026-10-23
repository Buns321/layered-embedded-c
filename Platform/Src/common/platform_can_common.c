/**
 * @file platform_can_common.c
 * @brief Family-independent CAN validation, errors and callback dispatch.
 */

#include "platform_can_backend.h"
#include "platform_error_internal.h"
#include "platform_validation_internal.h"
#include <stddef.h>

static volatile uint32_t can_error[PLATFORM_CAN_INSTANCE_COUNT];
static platform_can_callbacks_t can_cb[PLATFORM_CAN_INSTANCE_COUNT];
static bool can_cb_registered[PLATFORM_CAN_INSTANCE_COUNT];

static Platform_StatusTypeDef platform_can_finish(
  Platform_CAN_Instance can,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &can_error[can],
      platform_error_from_status(status)
    );
  }
  if (status == Platform_TIMEOUT) {
    platform_error_record(&can_error[can], PLATFORM_CAN_ERROR_TIMEOUT);
  }
  return status;
}

static Platform_StatusTypeDef platform_can_invalid(
  Platform_CAN_Instance can
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_record(
      &can_error[can],
      PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_CAN_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

static bool platform_can_frame_is_valid(const Platform_CAN_Frame *frame) {
  if (
    frame == NULL ||
    (frame->id_type != PLATFORM_CAN_ID_STANDARD &&
     frame->id_type != PLATFORM_CAN_ID_EXTENDED)
  ) {
    return false;
  }
  return platform_can_frame_values_are_valid(
    frame->id_type == PLATFORM_CAN_ID_EXTENDED,
    frame->id,
    frame->dlc
  );
}

static bool platform_can_filter_is_valid(
  const Platform_CAN_FilterConfig *config
) {
  if (
    config == NULL ||
    (config->mode != PLATFORM_CAN_FILTER_MASK &&
     config->mode != PLATFORM_CAN_FILTER_LIST) ||
    (config->fifo != PLATFORM_CAN_FIFO_0 &&
     config->fifo != PLATFORM_CAN_FIFO_1) ||
    (config->id_type != PLATFORM_CAN_ID_STANDARD &&
     config->id_type != PLATFORM_CAN_ID_EXTENDED)
  ) {
    return false;
  }

  const bool extended = config->id_type == PLATFORM_CAN_ID_EXTENDED;
  if (!platform_can_identifier_is_valid(extended, config->id)) {
    return false;
  }
  if (config->mode == PLATFORM_CAN_FILTER_LIST) {
    return platform_can_identifier_is_valid(extended, config->id2);
  }
  return platform_can_identifier_is_valid(extended, config->mask);
}

static bool platform_can_notifications_are_valid(uint32_t notifications) {
  const uint32_t all_notifications =
    PLATFORM_CAN_NOTIFY_RX_FIFO0_PENDING |
    PLATFORM_CAN_NOTIFY_RX_FIFO1_PENDING |
    PLATFORM_CAN_NOTIFY_RX_FIFO0_FULL |
    PLATFORM_CAN_NOTIFY_RX_FIFO1_FULL |
    PLATFORM_CAN_NOTIFY_TX_MAILBOX_EMPTY |
    PLATFORM_CAN_NOTIFY_ERROR;
  return notifications != 0U &&
         (notifications & ~all_notifications) == 0U;
}

void platform_can_dispatch_event(
  Platform_CAN_Instance can,
  const Platform_CAN_Event *event
) {
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    event == NULL ||
    !can_cb_registered[can]
  ) {
    return;
  }
  if (can_cb[can].callback != NULL) {
    can_cb[can].callback(can, event, can_cb[can].context);
  }
}

Platform_StatusTypeDef Platform_CAN_ConfigureFilter(
  Platform_CAN_Instance can,
  const Platform_CAN_FilterConfig *config
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_filter_is_valid(config)
  ) {
    return platform_can_invalid(can);
  }

  Platform_StatusTypeDef status =
    platform_can_backend_configure_filter(can, config);
  if (status == Platform_INVALID_ARGUMENT) {
    platform_can_invalid(can);
  } else {
    status = platform_can_finish(can, status);
  }
  return status;
}

Platform_StatusTypeDef Platform_CAN_Start(Platform_CAN_Instance can) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (!platform_can_backend_is_valid(can)) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(can, platform_can_backend_start(can));
}

Platform_StatusTypeDef Platform_CAN_Stop(Platform_CAN_Instance can) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (!platform_can_backend_is_valid(can)) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(can, platform_can_backend_stop(can));
}

Platform_StatusTypeDef Platform_CAN_Transmit(
  Platform_CAN_Instance can,
  const Platform_CAN_Frame *frame,
  uint32_t timeout
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_frame_is_valid(frame)
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_transmit(can, frame, timeout)
  );
}

Platform_StatusTypeDef Platform_CAN_Receive(
  Platform_CAN_Instance can,
  Platform_CAN_FIFO fifo,
  Platform_CAN_Frame *frame,
  uint32_t timeout
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    (fifo != PLATFORM_CAN_FIFO_0 && fifo != PLATFORM_CAN_FIFO_1) ||
    frame == NULL
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_receive(can, fifo, frame, timeout)
  );
}

Platform_StatusTypeDef Platform_CAN_ActivateNotification(
  Platform_CAN_Instance can,
  uint32_t notifications
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_notifications_are_valid(notifications)
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_activate_notification(can, notifications)
  );
}

Platform_StatusTypeDef Platform_CAN_DeactivateNotification(
  Platform_CAN_Instance can,
  uint32_t notifications
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    !platform_can_backend_is_valid(can) ||
    !platform_can_notifications_are_valid(notifications)
  ) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(
    can,
    platform_can_backend_deactivate_notification(can, notifications)
  );
}

uint32_t Platform_CAN_GetLastError(Platform_CAN_Instance can) {
  if ((uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT) {
    return PLATFORM_COMMON_ERROR_PARAMETER | PLATFORM_CAN_ERROR_PARAMETER;
  }
  return can_error[can] | platform_can_backend_get_error(can);
}

Platform_StatusTypeDef Platform_CAN_RegisterCallbacks(
  Platform_CAN_Instance can,
  const platform_can_callbacks_t *callbacks
) {
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_can_backend_is_valid(can)
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (can_cb_registered[can]) {
    return Platform_BUSY;
  }

  can_cb[can] = *callbacks;
  can_cb_registered[can] = true;
  return Platform_OK;
}
