/**
 * @file platform_uart_common.c
 * @brief 与芯片系列无关的 UART 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_uart_backend.h"
#include <stddef.h>

static volatile uint32_t uart_error[PLATFORM_UART_INSTANCE_COUNT];
static platform_uart_callback_binding_t uart_cb[PLATFORM_UART_INSTANCE_COUNT];
static bool uart_cb_registered[PLATFORM_UART_INSTANCE_COUNT];

static platform_status_t platform_uart_finish(
  platform_uart_instance_t uart,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &uart_error[uart],
      platform_error_from_status(status)
    );
  }
  return status;
}

static bool platform_uart_validate(
  platform_uart_instance_t uart,
  const void *data,
  uint16_t size
) {
  return (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
         platform_uart_backend_is_valid(uart) &&
         data != NULL &&
         size != 0U;
}

static platform_status_t platform_uart_invalid(
  platform_uart_instance_t uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_record(
      &uart_error[uart],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

void platform_uart_dispatch_event(
  platform_uart_instance_t uart,
  platform_uart_event_type_t type
) {
  if (
    (uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT ||
    !uart_cb_registered[uart]
  ) {
    return;
  }

  platform_uart_event_t event = {
    .type = type,
    .error = type == PLATFORM_UART_EVENT_ERROR
               ? platform_uart_backend_get_error(uart)
               : PLATFORM_UART_ERROR_NONE,
  };
  if (uart_cb[uart].callback != NULL) {
    uart_cb[uart].callback(uart, &event, uart_cb[uart].context);
  }
}

platform_status_t Platform_UART_Receive(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_receive(uart, pdata, size, timeout)
  );
}

platform_status_t Platform_UART_Transmit(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size,
  uint32_t timeout
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_transmit(uart, pdata, size, timeout)
  );
}

platform_status_t Platform_UART_ReceiveIT(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_receive_it(uart, pdata, size)
  );
}

platform_status_t Platform_UART_TransmitIT(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_transmit_it(uart, pdata, size)
  );
}

platform_status_t Platform_UART_ReceiveDMA(
  platform_uart_instance_t uart,
  uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_receive_dma(uart, pdata, size)
  );
}

platform_status_t Platform_UART_TransmitDMA(
  platform_uart_instance_t uart,
  const uint8_t *pdata,
  uint16_t size
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_validate(uart, pdata, size)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_transmit_dma(uart, pdata, size)
  );
}

platform_status_t Platform_UART_AbortReceive(
  platform_uart_instance_t uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_abort_receive(uart)
  );
}

platform_status_t Platform_UART_AbortTransmit(
  platform_uart_instance_t uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(
    uart,
    platform_uart_backend_abort_transmit(uart)
  );
}

platform_status_t Platform_UART_Abort(platform_uart_instance_t uart) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(uart, platform_uart_backend_abort(uart));
}

platform_status_t Platform_UART_ClearBuffer(
  platform_uart_instance_t uart
) {
  return Platform_UART_AbortReceive(uart);
}

uint32_t Platform_UART_GetLastError(platform_uart_instance_t uart) {
  if ((uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return uart_error[uart] | platform_uart_backend_get_error(uart);
}

platform_status_t Platform_UART_RegisterCallback(
  platform_uart_instance_t uart,
  const platform_uart_callback_binding_t *callbacks
) {
  if (
    (uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_uart_backend_is_valid(uart)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (uart_cb_registered[uart]) {
    return PLATFORM_STATUS_BUSY;
  }

  uart_cb[uart] = *callbacks;
  uart_cb_registered[uart] = true;
  return PLATFORM_STATUS_OK;
}
