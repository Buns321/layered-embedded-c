#include "platform_encoder_backend.h"
#include "platform_encoder.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_counter;
static uint8_t s_counter_bits;

bool platform_encoder_backend_is_valid(
  platform_encoder_instance_t encoder
) {
  return encoder == PLATFORM_ENCODER_TIM2;
}

platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
) {
  (void)encoder;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
) {
  (void)encoder;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_encoder_backend_get_counter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
) {
  (void)encoder;
  *counter = s_counter;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_encoder_backend_set_counter(
  platform_encoder_instance_t encoder,
  uint32_t counter
) {
  (void)encoder;
  s_counter = counter;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_encoder_backend_get_counter_bits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
) {
  (void)encoder;
  *counter_bits = s_counter_bits;
  return PLATFORM_STATUS_OK;
}

static void test_encoder_contract(void) {
  s_counter = 0x12345678U;
  s_counter_bits = 32U;

  uint32_t counter = 0U;
  uint8_t bits = 0U;
  assert(Platform_Encoder_GetCounterBits(
           PLATFORM_ENCODER_TIM2,
           &bits
         ) == PLATFORM_STATUS_OK);
  assert(bits == 32U);
  assert(Platform_Encoder_GetCounter(
           PLATFORM_ENCODER_TIM2,
           &counter
         ) == PLATFORM_STATUS_OK);
  assert(counter == s_counter);
  assert(Platform_Encoder_SetCounter(
           PLATFORM_ENCODER_TIM2,
           7U
         ) == PLATFORM_STATUS_OK);
  assert(s_counter == 7U);
  assert(Platform_Encoder_Start(PLATFORM_ENCODER_TIM2) == PLATFORM_STATUS_OK);
  assert(Platform_Encoder_Stop(PLATFORM_ENCODER_TIM2) == PLATFORM_STATUS_OK);

  assert(Platform_Encoder_GetCounter(
           PLATFORM_ENCODER_TIM2,
           NULL
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_Encoder_GetLastError(PLATFORM_ENCODER_TIM2) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

int main(void) {
  test_encoder_contract();
  return 0;
}
