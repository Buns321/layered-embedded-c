#include "platform_gpio_backend.h"
#include "platform_gpio.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint16_t s_last_pin;
static platform_gpio_pin_state_t s_last_state;

bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port) {
  return port == PLATFORM_GPIO_PORT_A;
}

platform_gpio_pin_state_t platform_gpio_backend_read(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  (void)pin;
  return PLATFORM_GPIO_PIN_SET;
}

void platform_gpio_backend_write(
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t state
) {
  (void)port;
  s_last_pin = pin;
  s_last_state = state;
}

void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  s_last_pin = pin;
}

platform_status_t platform_gpio_backend_lock(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  (void)pin;
  return PLATFORM_STATUS_OK;
}

static void test_gpio_contract(void) {
  assert(Platform_GPIO_ReadPin(PLATFORM_GPIO_PORT_A, 1U) ==
         PLATFORM_GPIO_PIN_SET);
  Platform_GPIO_WritePin(
    PLATFORM_GPIO_PORT_A,
    2U,
    PLATFORM_GPIO_PIN_RESET
  );
  assert(s_last_pin == 2U);
  assert(s_last_state == PLATFORM_GPIO_PIN_RESET);

  assert(Platform_GPIO_ReadPin((platform_gpio_port_t)99U, 1U) ==
         PLATFORM_GPIO_PIN_RESET);
  assert((Platform_GPIO_GetLastError((platform_gpio_port_t)99U) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

int main(void) {
  test_gpio_contract();
  return 0;
}
