/**
 * @file uart_contract_test.c
 * @brief UART Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_uart_backend.h"
#include "platform_uart.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static platform_uart_event_t s_last_event;

bool platform_uart_backend_is_valid(platform_uart_instance_t uart) {
  return uart == PLATFORM_UART_1;
}

platform_status_t platform_uart_backend_receive(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_transmit(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout;
  return s_transmit_status;
}

platform_status_t platform_uart_backend_receive_it(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_transmit_it(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_receive_dma(
  platform_uart_instance_t uart,
  uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_transmit_dma(
  platform_uart_instance_t uart,
  const uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_abort_receive(
  platform_uart_instance_t uart
) {
  (void)uart;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_abort_transmit(
  platform_uart_instance_t uart
) {
  (void)uart;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_uart_backend_abort(
  platform_uart_instance_t uart
) {
  (void)uart;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_uart_backend_get_error(platform_uart_instance_t uart) {
  (void)uart;
  return s_backend_error;
}

static void uart_callback(
  platform_uart_instance_t uart,
  const platform_uart_event_t *event,
  void *context
) {
  assert(uart == PLATFORM_UART_1);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

static void test_uart_contract(void) {
  s_transmit_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_UART_ERROR_NONE;
  s_event_count = 0U;

  uint8_t byte = 0U;
  assert(Platform_UART_Transmit(PLATFORM_UART_1, NULL, 1U, 10U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_UART_GetLastError(PLATFORM_UART_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  s_transmit_status = PLATFORM_STATUS_ERROR;
  assert(Platform_UART_Transmit(PLATFORM_UART_1, &byte, 1U, 10U) ==
         PLATFORM_STATUS_ERROR);
  assert((Platform_UART_GetLastError(PLATFORM_UART_1) &
          PLATFORM_ERROR_HAL) != 0U);

  s_transmit_status = PLATFORM_STATUS_OK;
  assert(Platform_UART_Transmit(PLATFORM_UART_1, &byte, 1U, 10U) ==
         PLATFORM_STATUS_OK);
  assert(Platform_UART_GetLastError(PLATFORM_UART_1) ==
         PLATFORM_ERROR_NONE);

  const platform_uart_callback_binding_t callbacks = {
    .callback = uart_callback,
    .context = &s_event_count,
  };
  assert(Platform_UART_RegisterCallback(PLATFORM_UART_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_UART_RegisterCallback(PLATFORM_UART_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  s_backend_error = PLATFORM_UART_ERROR_OVERRUN;
  platform_uart_dispatch_event(PLATFORM_UART_1, PLATFORM_UART_EVENT_ERROR);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_UART_EVENT_ERROR);
  assert(s_last_event.error == PLATFORM_UART_ERROR_OVERRUN);

  platform_uart_dispatch_event(PLATFORM_UART_1, PLATFORM_UART_EVENT_TX_COMPLETE);
  assert(s_event_count == 2U);
  assert(s_last_event.type == PLATFORM_UART_EVENT_TX_COMPLETE);
  assert(s_last_event.error == PLATFORM_UART_ERROR_NONE);

  assert((Platform_UART_GetLastError((platform_uart_instance_t)99U) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

int main(void) {
  test_uart_contract();
  return 0;
}
