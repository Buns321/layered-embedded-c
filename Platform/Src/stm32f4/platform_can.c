/**
 * @file platform_can.c
 * @brief STM32F4 HAL backend for the common CAN implementation.
 */

#include "can.h"
#include "platform_can_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include <string.h>

static CAN_HandleTypeDef *platform_can_to_hal(Platform_CAN_Instance can) {
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
  CAN_HandleTypeDef *hcan,
  Platform_CAN_Instance *can
) {
  if (hcan == NULL || can == NULL) {
    return false;
  }
#if PLATFORM_ENABLE_CAN1
  if (hcan == &hcan1) {
    *can = PLATFORM_CAN_1;
    return true;
  }
#endif
#if PLATFORM_ENABLE_CAN2
  if (hcan == &hcan2) {
    *can = PLATFORM_CAN_2;
    return true;
  }
#endif
  return false;
}

static uint32_t platform_can_filter_identifier(
  const Platform_CAN_FilterConfig *config,
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
  const Platform_CAN_FilterConfig *config
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
  const Platform_CAN_Frame *frame,
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
  Platform_CAN_Frame *frame
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

static Platform_StatusTypeDef platform_can_wait_tx_mailbox(
  CAN_HandleTypeDef *hcan,
  uint32_t timeout
) {
  const uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetTxMailboxesFreeLevel(hcan) == 0U) {
    if (
      timeout != PLATFORM_TIMEOUT_INFINITE &&
      (HAL_GetTick() - start) >= timeout
    ) {
      return Platform_TIMEOUT;
    }
  }
  return Platform_OK;
}

static Platform_StatusTypeDef platform_can_wait_rx(
  CAN_HandleTypeDef *hcan,
  uint32_t fifo,
  uint32_t timeout
) {
  const uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetRxFifoFillLevel(hcan, fifo) == 0U) {
    if (
      timeout != PLATFORM_TIMEOUT_INFINITE &&
      (HAL_GetTick() - start) >= timeout
    ) {
      return Platform_TIMEOUT;
    }
  }
  return Platform_OK;
}

bool platform_can_backend_is_valid(Platform_CAN_Instance can) {
  return (uint32_t)can < PLATFORM_CAN_INSTANCE_COUNT &&
         platform_can_to_hal(can) != NULL;
}

Platform_StatusTypeDef platform_can_backend_configure_filter(
  Platform_CAN_Instance can,
  const Platform_CAN_FilterConfig *config
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
    return Platform_INVALID_ARGUMENT;
  }

  CAN_HandleTypeDef *hcan = platform_can_to_hal(can);
  if (hcan == NULL) {
    return Platform_INVALID_ARGUMENT;
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
  return platform_from_hal_status(HAL_CAN_ConfigFilter(hcan, &filter));
}

Platform_StatusTypeDef platform_can_backend_start(
  Platform_CAN_Instance can
) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_Start(h));
}

Platform_StatusTypeDef platform_can_backend_stop(
  Platform_CAN_Instance can
) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_Stop(h));
}

Platform_StatusTypeDef platform_can_backend_transmit(
  Platform_CAN_Instance can,
  const Platform_CAN_Frame *frame,
  uint32_t timeout
) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  Platform_StatusTypeDef status =
    platform_can_wait_tx_mailbox(h, timeout);
  if (status != Platform_OK) {
    return status;
  }
  CAN_TxHeaderTypeDef header;
  platform_can_to_hal_header(frame, &header);
  uint32_t mailbox = 0U;
  return platform_from_hal_status(
    HAL_CAN_AddTxMessage(h, &header, frame->data, &mailbox)
  );
}

Platform_StatusTypeDef platform_can_backend_receive(
  Platform_CAN_Instance can,
  Platform_CAN_FIFO fifo,
  Platform_CAN_Frame *frame,
  uint32_t timeout
) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  if (h == NULL) {
    return Platform_INVALID_ARGUMENT;
  }
  const uint32_t hal_fifo = fifo == PLATFORM_CAN_FIFO_0
                              ? CAN_RX_FIFO0
                              : CAN_RX_FIFO1;
  Platform_StatusTypeDef status = platform_can_wait_rx(h, hal_fifo, timeout);
  if (status != Platform_OK) {
    return status;
  }
  CAN_RxHeaderTypeDef header;
  memset(frame, 0, sizeof(*frame));
  status = platform_from_hal_status(
    HAL_CAN_GetRxMessage(h, hal_fifo, &header, frame->data)
  );
  if (status == Platform_OK) {
    platform_can_from_hal_header(&header, frame);
  }
  return status;
}

Platform_StatusTypeDef platform_can_backend_activate_notification(
  Platform_CAN_Instance can,
  uint32_t notifications
) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_ActivateNotification(
                       h,
                       platform_can_notifications(notifications)
                     ));
}

Platform_StatusTypeDef platform_can_backend_deactivate_notification(
  Platform_CAN_Instance can,
  uint32_t notifications
) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  return h == NULL ? Platform_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_DeactivateNotification(
                       h,
                       platform_can_notifications(notifications)
                     ));
}

uint32_t platform_can_backend_get_error(Platform_CAN_Instance can) {
  CAN_HandleTypeDef *h = platform_can_to_hal(can);
  return h == NULL ? PLATFORM_CAN_ERROR_PARAMETER
                   : platform_can_map_error(HAL_CAN_GetError(h));
}

static void platform_can_dispatch_tx(
  CAN_HandleTypeDef *hcan,
  uint8_t mailbox,
  Platform_CAN_EventType type
) {
  Platform_CAN_Instance can;
  if (!platform_hal_to_can(hcan, &can)) {
    return;
  }
  const Platform_CAN_Event event = {
    .type = type,
    .mailbox = mailbox,
  };
  platform_can_dispatch_event(can, &event);
}

static void platform_can_dispatch_rx(
  CAN_HandleTypeDef *hcan,
  Platform_CAN_FIFO fifo,
  Platform_CAN_EventType type
) {
  Platform_CAN_Instance can;
  if (!platform_hal_to_can(hcan, &can)) {
    return;
  }
  const uint32_t hal_fifo = fifo == PLATFORM_CAN_FIFO_0
                              ? CAN_RX_FIFO0
                              : CAN_RX_FIFO1;
  CAN_RxHeaderTypeDef header;
  Platform_CAN_Event event = {
    .type = type,
    .fifo = fifo,
  };
  if (
    HAL_CAN_GetRxMessage(hcan, hal_fifo, &header, event.frame.data) == HAL_OK
  ) {
    platform_can_from_hal_header(&header, &event.frame);
  } else {
    event.type = PLATFORM_CAN_EVENT_ERROR;
    event.error = platform_can_backend_get_error(can);
  }
  platform_can_dispatch_event(can, &event);
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_tx(hcan, 0U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_tx(hcan, 1U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_tx(hcan, 2U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_tx(hcan, 0U, PLATFORM_CAN_EVENT_TX_ABORT);
}

void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_tx(hcan, 1U, PLATFORM_CAN_EVENT_TX_ABORT);
}

void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_tx(hcan, 2U, PLATFORM_CAN_EVENT_TX_ABORT);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_rx(
    hcan,
    PLATFORM_CAN_FIFO_0,
    PLATFORM_CAN_EVENT_RX_PENDING
  );
}

void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_rx(
    hcan,
    PLATFORM_CAN_FIFO_1,
    PLATFORM_CAN_EVENT_RX_PENDING
  );
}

void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_rx(
    hcan,
    PLATFORM_CAN_FIFO_0,
    PLATFORM_CAN_EVENT_RX_FULL
  );
}

void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *hcan) {
  platform_can_dispatch_rx(
    hcan,
    PLATFORM_CAN_FIFO_1,
    PLATFORM_CAN_EVENT_RX_FULL
  );
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan) {
  Platform_CAN_Instance can;
  if (!platform_hal_to_can(hcan, &can)) {
    return;
  }
  const Platform_CAN_Event event = {
    .type = PLATFORM_CAN_EVENT_ERROR,
    .error = platform_can_backend_get_error(can),
  };
  platform_can_dispatch_event(can, &event);
}
