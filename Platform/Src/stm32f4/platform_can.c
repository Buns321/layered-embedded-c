/**
 * @file platform_can.c
 * @brief STM32F4 公共 CAN 实现的 HAL 后端。
 * @ingroup platform
 */

#include "can.h"
#include "platform_can_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static CAN_HandleTypeDef *platform_can_to_hal(platform_can_instance_t can) {
  switch (can) {
#if PLATFORM_ENABLE_CAN1
    case PLATFORM_CAN_1: return &hcan1;
#endif
#if PLATFORM_ENABLE_CAN2
    case PLATFORM_CAN_2: return &hcan2;
#endif
    default: return NULL;
  }
}

static bool platform_hal_to_can(
  CAN_HandleTypeDef *can_h,
  platform_can_instance_t *can
) {
  if (can_h == NULL || can == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_CAN1
  if (can_h == &hcan1) {
    *can = PLATFORM_CAN_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_CAN2
  if (can_h == &hcan2) {
    *can = PLATFORM_CAN_2;
    return true;
  }
#endif
  return false;
}

static uint32_t platform_can_filter_identifier(
  const platform_can_filter_config_t *config,
  uint32_t identifier
) {
  if (config->id_type == PLATFORM_CAN_ID_EXTENDED) {
    return (identifier << 3U) | 0x00000004UL |
           (config->remote ? 0x00000002UL : 0U);
  }
  return (identifier << 21U) |
         (config->remote ? 0x00000002UL : 0U);
}

static uint32_t platform_can_filter_mask(
  const platform_can_filter_config_t *config
) {
  if (config->id_type == PLATFORM_CAN_ID_EXTENDED) {
    return (config->mask << 3U) | 0x00000006UL;
  }
  return (config->mask << 21U) | 0x00000006UL;
}

static uint32_t platform_can_notifications(uint32_t notifications) {
  uint32_t hal_notifications = 0U;
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO0_PENDING) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO0_MSG_PENDING;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO1_PENDING) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO1_MSG_PENDING;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO0_FULL) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO0_FULL;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO1_FULL) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO1_FULL;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_TX_MAILBOX_EMPTY) != 0U) {
    hal_notifications |= CAN_IT_TX_MAILBOX_EMPTY;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_ERROR) != 0U) {
    hal_notifications |= CAN_IT_ERROR;
  }
  return hal_notifications;
}

static uint32_t platform_can_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_CAN_ERROR_NONE;
  if ((hal_error & HAL_CAN_ERROR_EWG) != 0U) {
    error |= PLATFORM_CAN_ERROR_WARNING;
  }
  if ((hal_error & HAL_CAN_ERROR_EPV) != 0U) {
    error |= PLATFORM_CAN_ERROR_PASSIVE;
  }
  if ((hal_error & HAL_CAN_ERROR_BOF) != 0U) {
    error |= PLATFORM_CAN_ERROR_BUS_OFF;
  }
  if ((hal_error & HAL_CAN_ERROR_STF) != 0U) {
    error |= PLATFORM_CAN_ERROR_STUFF;
  }
  if ((hal_error & HAL_CAN_ERROR_FOR) != 0U) {
    error |= PLATFORM_CAN_ERROR_FORM;
  }
  if ((hal_error & HAL_CAN_ERROR_ACK) != 0U) {
    error |= PLATFORM_CAN_ERROR_ACK;
  }
  if ((hal_error & HAL_CAN_ERROR_BR) != 0U) {
    error |= PLATFORM_CAN_ERROR_BIT_RECESSIVE;
  }
  if ((hal_error & HAL_CAN_ERROR_BD) != 0U) {
    error |= PLATFORM_CAN_ERROR_BIT_DOMINANT;
  }
  if ((hal_error & HAL_CAN_ERROR_CRC) != 0U) {
    error |= PLATFORM_CAN_ERROR_CRC;
  }
  if ((hal_error & HAL_CAN_ERROR_RX_FOV0) != 0U) {
    error |= PLATFORM_CAN_ERROR_RX_FIFO0_OVERRUN;
  }
  if ((hal_error & HAL_CAN_ERROR_RX_FOV1) != 0U) {
    error |= PLATFORM_CAN_ERROR_RX_FIFO1_OVERRUN;
  }
  if (((hal_error & HAL_CAN_ERROR_TX_ALST0) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_ALST1) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_ALST2) != 0U)) {
    error |= PLATFORM_CAN_ERROR_TX_ARBITRATION_LOST;
  }
  if (((hal_error & HAL_CAN_ERROR_TX_TERR0) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_TERR1) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_TERR2) != 0U)) {
    error |= PLATFORM_CAN_ERROR_TX_FAILURE;
  }
  if ((hal_error & HAL_CAN_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_CAN_ERROR_TIMEOUT;
  }
#ifdef HAL_CAN_ERROR_NOT_INITIALIZED
  if ((hal_error & HAL_CAN_ERROR_NOT_INITIALIZED) != 0U) {
    error |= PLATFORM_CAN_ERROR_STATE;
  }
#endif
#ifdef HAL_CAN_ERROR_NOT_READY
  if ((hal_error & HAL_CAN_ERROR_NOT_READY) != 0U) {
    error |= PLATFORM_CAN_ERROR_STATE;
  }
#endif
#ifdef HAL_CAN_ERROR_NOT_STARTED
  if ((hal_error & HAL_CAN_ERROR_NOT_STARTED) != 0U) {
    error |= PLATFORM_CAN_ERROR_STATE;
  }
#endif
#ifdef HAL_CAN_ERROR_PARAM
  if ((hal_error & HAL_CAN_ERROR_PARAM) != 0U) {
    error |= PLATFORM_CAN_ERROR_PARAMETER;
  }
#endif
#ifdef HAL_CAN_ERROR_INTERNAL
  if ((hal_error & HAL_CAN_ERROR_INTERNAL) != 0U) {
    error |= PLATFORM_CAN_ERROR_INTERNAL;
  }
#endif
#ifdef HAL_CAN_ERROR_INVALID_CALLBACK
  if ((hal_error & HAL_CAN_ERROR_INVALID_CALLBACK) != 0U) {
    error |= PLATFORM_CAN_ERROR_PARAMETER;
  }
#endif

  uint32_t known_errors =
    HAL_CAN_ERROR_EWG |
    HAL_CAN_ERROR_EPV |
    HAL_CAN_ERROR_BOF |
    HAL_CAN_ERROR_STF |
    HAL_CAN_ERROR_FOR |
    HAL_CAN_ERROR_ACK |
    HAL_CAN_ERROR_BR |
    HAL_CAN_ERROR_BD |
    HAL_CAN_ERROR_CRC |
    HAL_CAN_ERROR_RX_FOV0 |
    HAL_CAN_ERROR_RX_FOV1 |
    HAL_CAN_ERROR_TX_ALST0 |
    HAL_CAN_ERROR_TX_ALST1 |
    HAL_CAN_ERROR_TX_ALST2 |
    HAL_CAN_ERROR_TX_TERR0 |
    HAL_CAN_ERROR_TX_TERR1 |
    HAL_CAN_ERROR_TX_TERR2 |
    HAL_CAN_ERROR_TIMEOUT;
#ifdef HAL_CAN_ERROR_NOT_INITIALIZED
  known_errors |= HAL_CAN_ERROR_NOT_INITIALIZED;
#endif
#ifdef HAL_CAN_ERROR_NOT_READY
  known_errors |= HAL_CAN_ERROR_NOT_READY;
#endif
#ifdef HAL_CAN_ERROR_NOT_STARTED
  known_errors |= HAL_CAN_ERROR_NOT_STARTED;
#endif
#ifdef HAL_CAN_ERROR_PARAM
  known_errors |= HAL_CAN_ERROR_PARAM;
#endif
#ifdef HAL_CAN_ERROR_INTERNAL
  known_errors |= HAL_CAN_ERROR_INTERNAL;
#endif
#ifdef HAL_CAN_ERROR_INVALID_CALLBACK
  known_errors |= HAL_CAN_ERROR_INVALID_CALLBACK;
#endif
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_CAN_ERROR_UNKNOWN;
  }
  return error;
}

static void platform_can_to_hal_header(
  const platform_can_frame_t *frame,
  CAN_TxHeaderTypeDef *header
) {
  memset(header, 0, sizeof(*header));
  header->DLC = frame->dlc;
  header->IDE = frame->id_type == PLATFORM_CAN_ID_EXTENDED
                  ? CAN_ID_EXT
                  : CAN_ID_STD;
  header->RTR = frame->remote ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  header->TransmitGlobalTime = DISABLE;
  if (frame->id_type == PLATFORM_CAN_ID_EXTENDED) {
    header->ExtId = frame->id;
  } else {
    header->StdId = frame->id;
  }
}

static void platform_can_from_hal_header(
  const CAN_RxHeaderTypeDef *header,
  platform_can_frame_t *frame
) {
  frame->dlc = (uint8_t)header->DLC;
  frame->id_type = header->IDE == CAN_ID_EXT
                     ? PLATFORM_CAN_ID_EXTENDED
                     : PLATFORM_CAN_ID_STANDARD;
  frame->remote = header->RTR == CAN_RTR_REMOTE;
  frame->id = frame->id_type == PLATFORM_CAN_ID_EXTENDED
                ? header->ExtId
                : header->StdId;
}

static platform_status_t platform_can_wait_tx_mailbox(
  CAN_HandleTypeDef *can_h,
  uint32_t timeout
) {
  const uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetTxMailboxesFreeLevel(can_h) == 0U) {
    if (
      timeout != PLATFORM_TIMEOUT_INFINITE &&
      (HAL_GetTick() - start) >= timeout
    ) {
      return PLATFORM_STATUS_TIMEOUT;
    }
  }
  return PLATFORM_STATUS_OK;
}

static platform_status_t platform_can_wait_rx(
  CAN_HandleTypeDef *can_h,
  uint32_t fifo,
  uint32_t timeout
) {
  const uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetRxFifoFillLevel(can_h, fifo) == 0U) {
    if (
      timeout != PLATFORM_TIMEOUT_INFINITE &&
      (HAL_GetTick() - start) >= timeout
    ) {
      return PLATFORM_STATUS_TIMEOUT;
    }
  }
  return PLATFORM_STATUS_OK;
}

bool platform_can_backend_is_valid(platform_can_instance_t can) {
  return (uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT &&
         platform_can_to_hal(can) != NULL;
}

platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
) {
#if PLATFORM_ENABLE_CAN1 && PLATFORM_ENABLE_CAN2
  const bool bank_is_valid =
    (can == PLATFORM_CAN_1 && config->bank <= 13U) ||
    (can == PLATFORM_CAN_2 && config->bank >= 14U && config->bank <= 27U);
#elif PLATFORM_ENABLE_CAN1
  const bool bank_is_valid =
    can == PLATFORM_CAN_1 && config->bank <= 13U;
#elif PLATFORM_ENABLE_CAN2
  const bool bank_is_valid =
    can == PLATFORM_CAN_2 && config->bank >= 14U && config->bank <= 27U;
#else
  const bool bank_is_valid = false;
#endif
  if (!bank_is_valid) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  if (can_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  const uint32_t id = platform_can_filter_identifier(config, config->id);
  const uint32_t secondary =
    config->mode == PLATFORM_CAN_FILTER_LIST
      ? platform_can_filter_identifier(config, config->id2)
      : platform_can_filter_mask(config);

  CAN_FilterTypeDef filter = {0};
  filter.FilterIdHigh = (uint16_t)(id >> 16U);
  filter.FilterIdLow = (uint16_t)(id & 0xFFFFU);
  filter.FilterMaskIdHigh = (uint16_t)(secondary >> 16U);
  filter.FilterMaskIdLow = (uint16_t)(secondary & 0xFFFFU);
  filter.FilterFIFOAssignment = config->fifo == PLATFORM_CAN_FIFO_0
                                  ? CAN_FILTER_FIFO0
                                  : CAN_FILTER_FIFO1;
  filter.FilterBank = config->bank;
  filter.FilterMode = config->mode == PLATFORM_CAN_FILTER_MASK
                        ? CAN_FILTERMODE_IDMASK
                        : CAN_FILTERMODE_IDLIST;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterActivation = config->enabled ? ENABLE : DISABLE;
  filter.SlaveStartFilterBank = 14U;
  return platform_from_hal_status(HAL_CAN_ConfigFilter(can_h, &filter));
}

platform_status_t platform_can_backend_start(
  platform_can_instance_t can
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_Start(can_h));
}

platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_Stop(can_h));
}

platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  if (can_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  platform_status_t status =
    platform_can_wait_tx_mailbox(can_h, timeout);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }
  CAN_TxHeaderTypeDef header;
  platform_can_to_hal_header(frame, &header);
  uint32_t mailbox = 0U;
  return platform_from_hal_status(
    HAL_CAN_AddTxMessage(can_h, &header, frame->data, &mailbox)
  );
}

platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  if (can_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  const uint32_t hal_fifo = fifo == PLATFORM_CAN_FIFO_0
                              ? CAN_RX_FIFO0
                              : CAN_RX_FIFO1;
  platform_status_t status = platform_can_wait_rx(can_h, hal_fifo, timeout);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }
  CAN_RxHeaderTypeDef header;
  memset(frame, 0, sizeof(*frame));
  status = platform_from_hal_status(
    HAL_CAN_GetRxMessage(can_h, hal_fifo, &header, frame->data)
  );
  if (status == PLATFORM_STATUS_OK) {
    platform_can_from_hal_header(&header, frame);
  }
  return status;
}

platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_ActivateNotification(
                       can_h,
                       platform_can_notifications(notifications)
                     ));
}

platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_DeactivateNotification(
                       can_h,
                       platform_can_notifications(notifications)
                     ));
}

uint32_t platform_can_backend_get_error(platform_can_instance_t can) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_CAN_ERROR_PARAMETER
                   : platform_can_map_error(HAL_CAN_GetError(can_h));
}

static void platform_can_dispatch_tx(
  CAN_HandleTypeDef *can_h,
  uint8_t mailbox,
  platform_can_event_type_t type
) {
  platform_can_instance_t can;
  if (!platform_hal_to_can(can_h, &can)) {
    return;
  }
  const platform_can_event_t event = {
    .type = type,
    .mailbox = mailbox,
  };
  platform_can_dispatch_event(can, &event);
}

static void platform_can_dispatch_rx(
  CAN_HandleTypeDef *can_h,
  platform_can_fifo_t fifo,
  platform_can_event_type_t type
) {
  platform_can_instance_t can;
  if (!platform_hal_to_can(can_h, &can)) {
    return;
  }
  const uint32_t hal_fifo = fifo == PLATFORM_CAN_FIFO_0
                              ? CAN_RX_FIFO0
                              : CAN_RX_FIFO1;
  CAN_RxHeaderTypeDef header;
  platform_can_event_t event = {
    .type = type,
    .fifo = fifo,
  };
  if (
    HAL_CAN_GetRxMessage(can_h, hal_fifo, &header, event.frame.data) == HAL_OK
  ) {
    platform_can_from_hal_header(&header, &event.frame);
  } else {
    event.type = PLATFORM_CAN_EVENT_ERROR;
    event.error = platform_can_backend_get_error(can);
  }
  platform_can_dispatch_event(can, &event);
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 0U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 1U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 2U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 0U, PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE);
}

void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 1U, PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE);
}

void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 2U, PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_0,
    PLATFORM_CAN_EVENT_RX_PENDING
  );
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_1,
    PLATFORM_CAN_EVENT_RX_PENDING
  );
}

void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_0,
    PLATFORM_CAN_EVENT_RX_FULL
  );
}

void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_1,
    PLATFORM_CAN_EVENT_RX_FULL
  );
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *can_h) {
  platform_can_instance_t can;
  if (!platform_hal_to_can(can_h, &can)) {
    return;
  }
  const platform_can_event_t event = {
    .type = PLATFORM_CAN_EVENT_ERROR,
    .error = platform_can_backend_get_error(can),
  };
  platform_can_dispatch_event(can, &event);
}
