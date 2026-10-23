//
// Created by Buns_ on 2026/10/23.
//

/**
 * @file platform_can.h
 * @brief HAL-free classic CAN interface.
 * @addtogroup platform
 * @{
 * @note CAN FD is outside this interface.
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CAN_H
#define LAYERED_EMBEDDED_C_PLATFORM_CAN_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

#if !PLATFORM_ENABLE_CAN1 && !PLATFORM_ENABLE_CAN2
#error "At least one Platform CAN instance must be enabled"
#endif

/** @brief Available classic CAN instances. */
typedef enum {
#if PLATFORM_ENABLE_CAN1
  PLATFORM_CAN_1 = 0U,
#endif
#if PLATFORM_ENABLE_CAN2
  PLATFORM_CAN_2 = 1U,
#endif
} Platform_CAN_Instance;

/** @brief Number of possible CAN identity slots. */
#define PLATFORM_CAN_INSTANCE_COUNT 2U

/** @brief CAN identifier format. */
typedef enum {
  PLATFORM_CAN_ID_STANDARD = 0U,
  PLATFORM_CAN_ID_EXTENDED,
} Platform_CAN_IdType;

/** @brief CAN receive FIFO selection. */
typedef enum {
  PLATFORM_CAN_FIFO_0 = 0U,
  PLATFORM_CAN_FIFO_1,
} Platform_CAN_FIFO;

/** @brief Classic CAN frame. */
typedef struct {
  uint32_t id;
  uint8_t dlc;
  Platform_CAN_IdType id_type;
  bool remote;
  uint8_t data[8];
} Platform_CAN_Frame;

/** @brief Filter matching mode. */
typedef enum {
  PLATFORM_CAN_FILTER_MASK = 0U,
  PLATFORM_CAN_FILTER_LIST,
} Platform_CAN_FilterMode;

/**
 * @brief 32-bit filter configuration.
 * @note id and mask contain only CAN identifier bits. For a standard filter,
 *       bits 10:0 are used. For an extended filter, bits 28:0 are used.
 *       The implementation adds IDE and RTR fields to the hardware layout.
 * @note In mask mode, mask is the acceptance mask. In list mode, the two
 *       identifiers are id and id2; both use id_type and remote.
 */
typedef struct {
  uint32_t bank;
  uint32_t id;
  uint32_t mask;
  uint32_t id2;
  Platform_CAN_FilterMode mode;
  Platform_CAN_FIFO fifo;
  Platform_CAN_IdType id_type;
  bool remote;
  bool enabled;
} Platform_CAN_FilterConfig;

/** @brief CAN interrupt notification bits. */
typedef enum {
  PLATFORM_CAN_NOTIFY_RX_FIFO0_PENDING = (1u << 0),
  PLATFORM_CAN_NOTIFY_RX_FIFO1_PENDING = (1u << 1),
  PLATFORM_CAN_NOTIFY_RX_FIFO0_FULL    = (1u << 2),
  PLATFORM_CAN_NOTIFY_RX_FIFO1_FULL    = (1u << 3),
  PLATFORM_CAN_NOTIFY_TX_MAILBOX_EMPTY = (1u << 4),
  PLATFORM_CAN_NOTIFY_ERROR            = (1u << 5),
} Platform_CAN_Notification;

/** @brief CAN error flags. More than one bit may be set. */
typedef enum {
  PLATFORM_CAN_ERROR_NONE                = 0u,
  PLATFORM_CAN_ERROR_WARNING             = (1u << 0),
  PLATFORM_CAN_ERROR_PASSIVE             = (1u << 1),
  PLATFORM_CAN_ERROR_BUS_OFF             = (1u << 2),
  PLATFORM_CAN_ERROR_STUFF               = (1u << 3),
  PLATFORM_CAN_ERROR_FORM                = (1u << 4),
  PLATFORM_CAN_ERROR_ACK                 = (1u << 5),
  PLATFORM_CAN_ERROR_BIT_RECESSIVE       = (1u << 6),
  PLATFORM_CAN_ERROR_BIT_DOMINANT        = (1u << 7),
  PLATFORM_CAN_ERROR_CRC                 = (1u << 8),
  PLATFORM_CAN_ERROR_RX_FIFO0_OVERRUN    = (1u << 9),
  PLATFORM_CAN_ERROR_RX_FIFO1_OVERRUN    = (1u << 10),
  PLATFORM_CAN_ERROR_TX_ARBITRATION_LOST = (1u << 11),
  PLATFORM_CAN_ERROR_TX_FAILURE          = (1u << 12),
  PLATFORM_CAN_ERROR_TIMEOUT             = (1u << 13),
  PLATFORM_CAN_ERROR_STATE               = (1u << 14),
  PLATFORM_CAN_ERROR_PARAMETER           = (1u << 15),
  PLATFORM_CAN_ERROR_INTERNAL            = (1u << 16),
  PLATFORM_CAN_ERROR_UNKNOWN             = (1u << 31),
} Platform_CAN_Error;

/** @brief Interrupt event type. */
typedef enum {
  PLATFORM_CAN_EVENT_TX_COMPLETE = 0U,
  PLATFORM_CAN_EVENT_TX_ABORT,
  PLATFORM_CAN_EVENT_RX_PENDING,
  PLATFORM_CAN_EVENT_RX_FULL,
  PLATFORM_CAN_EVENT_ERROR,
} Platform_CAN_EventType;

/**
 * @brief Interrupt event delivered to the registered callback.
 * @note The event and its frame are valid only for the duration of the
 *       callback. Copy any required data before returning.
 */
typedef struct {
  Platform_CAN_EventType type;
  uint8_t mailbox;
  Platform_CAN_FIFO fifo;
  Platform_CAN_Frame frame;
  uint32_t error;
} Platform_CAN_Event;

typedef void (*Platform_CAN_Callback)(
  Platform_CAN_Instance can,
  const Platform_CAN_Event *event,
  void *context
);

/**
 * @brief Configure one 32-bit filter bank.
 * @param can CAN instance.
 * @param config Filter configuration.
 * @pre Call before Platform_CAN_Start().
 * @note id and mask are CAN identifier bits, not raw hardware registers.
 * @retval Platform_ERROR Invalid instance, bank, mode, FIFO, or identifier.
 */
Platform_StatusTypeDef Platform_CAN_ConfigureFilter(
  Platform_CAN_Instance can,
  const Platform_CAN_FilterConfig *config
);

/**
 * @brief Start the CAN peripheral.
 * @param can CAN instance.
 */
Platform_StatusTypeDef Platform_CAN_Start(Platform_CAN_Instance can);

/**
 * @brief Stop the CAN peripheral.
 * @param can CAN instance.
 */
Platform_StatusTypeDef Platform_CAN_Stop(Platform_CAN_Instance can);

/**
 * @brief Enqueue one classic CAN frame.
 * @param can CAN instance.
 * @param frame Frame to enqueue.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE while
 *                waiting for a free transmit mailbox.
 * @retval Platform_OK Frame queued.
 * @retval Platform_BUSY HAL reported a busy state.
 * @retval Platform_TIMEOUT No mailbox became free.
 * @retval Platform_ERROR Invalid frame or peripheral state.
 * @note Call from task context. The implementation does not sleep while
 *       polling for a mailbox.
 */
Platform_StatusTypeDef Platform_CAN_Transmit(
  Platform_CAN_Instance can,
  const Platform_CAN_Frame *frame,
  uint32_t timeout
);

/**
 * @brief Receive one frame from a FIFO.
 * @param can CAN instance.
 * @param fifo Receive FIFO.
 * @param frame Destination frame.
 * @param timeout Timeout in milliseconds or PLATFORM_TIMEOUT_INFINITE.
 * @retval Platform_OK Frame copied to frame.
 * @retval Platform_TIMEOUT No frame arrived before the timeout.
 * @retval Platform_ERROR Invalid argument or HAL failure.
 * @note Call from task context. The implementation does not sleep while
 *       polling for a frame.
 */
Platform_StatusTypeDef Platform_CAN_Receive(
  Platform_CAN_Instance can,
  Platform_CAN_FIFO fifo,
  Platform_CAN_Frame *frame,
  uint32_t timeout
);

/**
 * @brief Enable selected CAN interrupt notifications.
 * @param can CAN instance.
 * @param notifications Platform_CAN_Notification bit mask.
 */
Platform_StatusTypeDef Platform_CAN_ActivateNotification(
  Platform_CAN_Instance can,
  uint32_t notifications
);

/**
 * @brief Disable selected CAN interrupt notifications.
 * @param can CAN instance.
 * @param notifications Platform_CAN_Notification bit mask.
 */
Platform_StatusTypeDef Platform_CAN_DeactivateNotification(
  Platform_CAN_Instance can,
  uint32_t notifications
);

/**
 * @brief Read the latest HAL CAN error snapshot.
 * @param can CAN instance.
 */
uint32_t Platform_CAN_GetLastError(Platform_CAN_Instance can);

/**
 * @brief Register CAN event callback.
 * @param can CAN instance.
 * @param callback Event callback.
 * @param context Caller-owned callback context.
 * @note Call once during initialization before enabling notifications.
 */
void Platform_CAN_RegisterCallbacks(
  Platform_CAN_Instance can,
  Platform_CAN_Callback callback,
  void *context
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_CAN_H
