#include "platform_uart_backend.h"
#include "platform_usart.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static Platform_StatusTypeDef s_transmit_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static Platform_UART_Event s_last_event;

bool platform_uart_backend_is_valid(Platform_UART_Instance uart) {
  return uart == PLATFORM_USART_1;
}

Platform_StatusTypeDef platform_uart_backend_receive(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)uart;
  (void)data;
  (void)size;
  (void)timeout;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_transmit(
  Platform_UART_Instance uart,
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

Platform_StatusTypeDef platform_uart_backend_receive_it(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_transmit_it(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_receive_dma(
  Platform_UART_Instance uart,
  uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_transmit_dma(
  Platform_UART_Instance uart,
  const uint8_t *data,
  uint16_t size
) {
  (void)uart;
  (void)data;
  (void)size;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_abort_receive(
  Platform_UART_Instance uart
) {
  (void)uart;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_abort_transmit(
  Platform_UART_Instance uart
) {
  (void)uart;
  return Platform_OK;
}

Platform_StatusTypeDef platform_uart_backend_abort(
  Platform_UART_Instance uart
) {
  (void)uart;
  return Platform_OK;
}

uint32_t platform_uart_backend_get_error(Platform_UART_Instance uart) {
  (void)uart;
  return s_backend_error;
}

static void uart_callback(
  Platform_UART_Instance uart,
  const Platform_UART_Event *event,
  void *context
) {
  assert(uart == PLATFORM_USART_1);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

static void test_uart_contract(void) {
  s_transmit_status = Platform_OK;
  s_backend_error = PLATFORM_UART_ERROR_NONE;
  s_event_count = 0U;

  uint8_t byte = 0U;
  assert(Platform_UART_Transmit(PLATFORM_USART_1, NULL, 1U, 10U) ==
         Platform_INVALID_ARGUMENT);
  assert((Platform_UART_GetLastError(PLATFORM_USART_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  s_transmit_status = Platform_ERROR;
  assert(Platform_UART_Transmit(PLATFORM_USART_1, &byte, 1U, 10U) ==
         Platform_ERROR);
  assert((Platform_UART_GetLastError(PLATFORM_USART_1) &
          PLATFORM_COMMON_ERROR_HAL) != 0U);

  s_transmit_status = Platform_OK;
  assert(Platform_UART_Transmit(PLATFORM_USART_1, &byte, 1U, 10U) ==
         Platform_OK);
  assert(Platform_UART_GetLastError(PLATFORM_USART_1) ==
         PLATFORM_COMMON_ERROR_NONE);

  const platform_uart_callbacks_t callbacks = {
    .callback = uart_callback,
    .context = &s_event_count,
  };
  assert(Platform_UART_RegisterCallbacks(PLATFORM_USART_1, &callbacks) ==
         Platform_OK);
  assert(Platform_UART_RegisterCallbacks(PLATFORM_USART_1, &callbacks) ==
         Platform_BUSY);

  s_backend_error = PLATFORM_UART_ERROR_OVERRUN;
  platform_uart_dispatch_event(PLATFORM_USART_1, PLATFORM_UART_EVENT_ERROR);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_UART_EVENT_ERROR);
  assert(s_last_event.error == PLATFORM_UART_ERROR_OVERRUN);

  platform_uart_dispatch_event(PLATFORM_USART_1, PLATFORM_UART_EVENT_TX_CPLT);
  assert(s_event_count == 2U);
  assert(s_last_event.type == PLATFORM_UART_EVENT_TX_CPLT);
  assert(s_last_event.error == PLATFORM_UART_ERROR_NONE);

  assert((Platform_UART_GetLastError((Platform_UART_Instance)99U) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);
}

int main(void) {
  test_uart_contract();
  return 0;
}
