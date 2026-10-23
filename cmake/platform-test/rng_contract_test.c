#include "platform_rng_backend.h"
#include "platform_rng.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_value;
static uint32_t s_backend_error;

bool platform_rng_backend_is_valid(platform_rng_instance_t rng) {
  return rng == PLATFORM_RNG_1;
}

platform_status_t platform_rng_backend_get_random(
  platform_rng_instance_t rng,
  uint32_t *value
) {
  (void)rng;
  *value = s_value;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_rng_backend_get_error(platform_rng_instance_t rng) {
  (void)rng;
  return s_backend_error;
}

static void test_rng_contract(void) {
  uint32_t value = 0U;
  s_value = 0x12345678U;
  s_backend_error = PLATFORM_RNG_ERROR_NONE;

  assert(Platform_RNG_GetRandom(PLATFORM_RNG_1, &value) == PLATFORM_STATUS_OK);
  assert(value == s_value);
  assert(Platform_RNG_GetRandom(PLATFORM_RNG_1, NULL) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_RNG_GetLastError(PLATFORM_RNG_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

int main(void) {
  test_rng_contract();
  return 0;
}
