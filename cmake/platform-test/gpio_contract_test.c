#include "platform_gpio_backend.h"
#include "platform_gpio.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint16_t s_last_pin;
static Platform_GPIO_Pin_State s_last_state;

bool platform_gpio_backend_is_valid_port(Platform_GPIO_Port port) {
  return port == PLATFORM_GPIO_PORT_A;
}

Platform_GPIO_Pin_State platform_gpio_backend_read(
  Platform_GPIO_Port port,
  uint16_t pin
) {
  (void)port;
  (void)pin;
  return PLATFORM_GPIO_PIN_SET;
}

void platform_gpio_backend_write(
  Platform_GPIO_Port port,
  uint16_t pin,
  Platform_GPIO_Pin_State state
) {
  (void)port;
  s_last_pin = pin;
  s_last_state = state;
}

void platform_gpio_backend_toggle(
  Platform_GPIO_Port port,
  uint16_t pin
) {
  (void)port;
  s_last_pin = pin;
}

Platform_StatusTypeDef platform_gpio_backend_lock(
  Platform_GPIO_Port port,
  uint16_t pin
) {
  (void)port;
  (void)pin;
  return Platform_OK;
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

  assert(Platform_GPIO_ReadPin((Platform_GPIO_Port)99U, 1U) ==
         PLATFORM_GPIO_PIN_RESET);
  assert((Platform_GPIO_GetLastError((Platform_GPIO_Port)99U) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);
}

int main(void) {
  test_gpio_contract();
  return 0;
}
