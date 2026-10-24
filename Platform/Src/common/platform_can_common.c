/**
 * @file platform_can_common.c
 * @brief 与芯片系列无关的 CAN 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_can_backend.h"
#include "platform_error_internal.h"
#include "platform_validation_internal.h"
#include <stddef.h>

static volatile uint32_t can_error[PLATFORM_CAN_INSTANCE_COUNT];
static platform_can_callback_binding_t can_cb[PLATFORM_CAN_INSTANCE_COUNT];
static bool can_cb_registered[PLATFORM_CAN_INSTANCE_COUNT];

static platform_status_t platform_can_finish(
  platform_can_instance_t can,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &can_error[can],
      platform_error_from_status(status)
    );
  }
  if (status == PLATFORM_STATUS_TIMEOUT) {
    platform_error_record(&can_error[can], PLATFORM_CAN_ERROR_TIMEOUT);
  }
  return status;
}

static platform_status_t platform_can_invalid(
  platform_can_instance_t can
) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_record(
      &can_error[can],
      PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_CAN_ERROR_PARAMETER
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

static bool platform_can_frame_is_valid(const platform_can_frame_t *frame) {
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
  const platform_can_filter_config_t *config
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
  platform_can_instance_t can,
  const platform_can_event_t *event
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

platform_status_t Platform_CAN_ConfigureFilter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
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

  platform_status_t status =
    platform_can_backend_configure_filter(can, config);
  if (status == PLATFORM_STATUS_INVALID_ARGUMENT) {
    platform_can_invalid(can);
  } else {
    status = platform_can_finish(can, status);
  }
  return status;
}

platform_status_t Platform_CAN_Start(platform_can_instance_t can) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (!platform_can_backend_is_valid(can)) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(can, platform_can_backend_start(can));
}

platform_status_t Platform_CAN_Stop(platform_can_instance_t can) {
  if ((uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT) {
    platform_error_reset(&can_error[can]);
  }
  if (!platform_can_backend_is_valid(can)) {
    return platform_can_invalid(can);
  }
  return platform_can_finish(can, platform_can_backend_stop(can));
}

platform_status_t Platform_CAN_Transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
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

platform_status_t Platform_CAN_Receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
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

platform_status_t Platform_CAN_ActivateNotification(
  platform_can_instance_t can,
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

platform_status_t Platform_CAN_DeactivateNotification(
  platform_can_instance_t can,
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

uint32_t Platform_CAN_GetLastError(platform_can_instance_t can) {
  if ((uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT | PLATFORM_CAN_ERROR_PARAMETER;
  }
  return can_error[can] | platform_can_backend_get_error(can);
}

platform_status_t Platform_CAN_RegisterCallback(
  platform_can_instance_t can,
  const platform_can_callback_binding_t *callbacks
) {
  if (
    (uint32_t)can >= PLATFORM_CAN_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_can_backend_is_valid(can)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (can_cb_registered[can]) {
    return PLATFORM_STATUS_BUSY;
  }

  can_cb[can] = *callbacks;
  can_cb_registered[can] = true;
  return PLATFORM_STATUS_OK;
}
