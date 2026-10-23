/**
 * @file platform_uart_common.c
 * @brief Family-independent UART validation, errors and callback dispatch.
 */

#include "platform_error_internal.h"
#include "platform_uart_backend.h"
#include <stddef.h>

static volatile uint32_t uart_error[PLATFORM_UART_INSTANCE_COUNT];
static platform_uart_callbacks_t uart_cb[PLATFORM_UART_INSTANCE_COUNT];
static bool uart_cb_registered[PLATFORM_UART_INSTANCE_COUNT];

static Platform_StatusTypeDef platform_uart_finish(
  Platform_UART_Instance uart,
  Platform_StatusTypeDef status
) {
  if (status != Platform_OK) {
    platform_error_record(
      &uart_error[uart],
      platform_error_from_status(status)
    );
  }
  return status;
}

static bool platform_uart_validate(
  Platform_UART_Instance uart,
  const void *data,
  uint16_t size
) {
  return (uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT &&
         platform_uart_backend_is_valid(uart) &&
         data != NULL &&
         size != 0U;
}

static Platform_StatusTypeDef platform_uart_invalid(
  Platform_UART_Instance uart
) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_record(
      &uart_error[uart],
      PLATFORM_COMMON_ERROR_PARAMETER
    );
  }
  return Platform_INVALID_ARGUMENT;
}

void platform_uart_dispatch_event(
  Platform_UART_Instance uart,
  Platform_UART_EventType type
) {
  if (
    (uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT ||
    !uart_cb_registered[uart]
  ) {
    return;
  }

  Platform_UART_Event event = {
    .type = type,
    .error = type == PLATFORM_UART_EVENT_ERROR
               ? platform_uart_backend_get_error(uart)
               : PLATFORM_UART_ERROR_NONE,
  };
  if (uart_cb[uart].callback != NULL) {
    uart_cb[uart].callback(uart, &event, uart_cb[uart].context);
  }
}

Platform_StatusTypeDef Platform_UART_Receive(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef Platform_UART_Transmit(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef Platform_UART_Receive_IT(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef Platform_UART_Transmit_IT(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef Platform_UART_Receive_DMA(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef Platform_UART_Transmit_DMA(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef Platform_UART_AbortReceive(
  Platform_UART_Instance uart
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

Platform_StatusTypeDef Platform_UART_AbortTransmit(
  Platform_UART_Instance uart
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

Platform_StatusTypeDef Platform_UART_Abort(Platform_UART_Instance uart) {
  if ((uint32_t)uart < PLATFORM_UART_INSTANCE_COUNT) {
    platform_error_reset(&uart_error[uart]);
  }
  if (!platform_uart_backend_is_valid(uart)) {
    return platform_uart_invalid(uart);
  }
  return platform_uart_finish(uart, platform_uart_backend_abort(uart));
}

Platform_StatusTypeDef Platform_UART_ClearBuffer(
  Platform_UART_Instance uart
) {
  return Platform_UART_AbortReceive(uart);
}

uint32_t Platform_UART_GetLastError(Platform_UART_Instance uart) {
  if ((uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT) {
    return PLATFORM_COMMON_ERROR_PARAMETER;
  }
  return uart_error[uart] | platform_uart_backend_get_error(uart);
}

Platform_StatusTypeDef Platform_UART_RegisterCallbacks(
  Platform_UART_Instance uart,
  const platform_uart_callbacks_t *callbacks
) {
  if (
    (uint32_t)uart >= PLATFORM_UART_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_uart_backend_is_valid(uart)
  ) {
    return Platform_INVALID_ARGUMENT;
  }
  if (uart_cb_registered[uart]) {
    return Platform_BUSY;
  }

  uart_cb[uart] = *callbacks;
  uart_cb_registered[uart] = true;
  return Platform_OK;
}
