#include "platform_exti.h"
#include "platform_exti_backend.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_callback_count;

static void exti_callback(uint16_t pin, void *context) {
  assert(pin == 0x0004U);
  assert(context == &s_callback_count);
  s_callback_count++;
}

static void test_exti_contract(void) {
  s_callback_count = 0U;
  const platform_exti_callbacks_t callbacks = {
    .exti_cb = exti_callback,
    .context = &s_callback_count,
  };

  assert(Platform_EXTI_RegisterCallbacks(0x0004U, &callbacks) == Platform_OK);
  assert(Platform_EXTI_RegisterCallbacks(0x0004U, &callbacks) ==
         Platform_BUSY);
  assert(Platform_EXTI_RegisterCallbacks(0U, &callbacks) ==
         Platform_INVALID_ARGUMENT);
  assert(Platform_EXTI_RegisterCallbacks(0x0004U, NULL) ==
         Platform_INVALID_ARGUMENT);

  platform_exti_dispatch_event(0x0004U);
  assert(s_callback_count == 1U);
  platform_exti_dispatch_event(0x0008U);
  assert(s_callback_count == 1U);
}

int main(void) {
  test_exti_contract();
  return 0;
}
