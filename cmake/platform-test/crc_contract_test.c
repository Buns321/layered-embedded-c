#include "platform_crc_backend.h"
#include "platform_crc.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static uint32_t s_result;

bool platform_crc_backend_is_valid(Platform_CRC_Instance crc) {
  return crc == PLATFORM_CRC_1;
}

Platform_StatusTypeDef platform_crc_backend_reset(
  Platform_CRC_Instance crc
) {
  (void)crc;
  return Platform_OK;
}

Platform_StatusTypeDef platform_crc_backend_accumulate(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  (void)crc;
  (void)words;
  (void)word_count;
  *result = s_result;
  return Platform_OK;
}

Platform_StatusTypeDef platform_crc_backend_calculate(
  Platform_CRC_Instance crc,
  const uint32_t *words,
  uint32_t word_count,
  uint32_t *result
) {
  return platform_crc_backend_accumulate(
    crc,
    words,
    word_count,
    result
  );
}

static void test_crc_contract(void) {
  const uint32_t words[2] = {1U, 2U};
  uint32_t result = 0U;
  s_result = 0xA5A5U;

  assert(Platform_CRC_Reset(PLATFORM_CRC_1) == Platform_OK);
  assert(Platform_CRC_AccumulateWords(
           PLATFORM_CRC_1,
           words,
           2U,
           &result
         ) == Platform_OK);
  assert(result == s_result);
  assert(Platform_CRC_CalculateWords(
           PLATFORM_CRC_1,
           NULL,
           2U,
           &result
         ) == Platform_INVALID_ARGUMENT);
  assert((Platform_CRC_GetLastError(PLATFORM_CRC_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);
}

int main(void) {
  test_crc_contract();
  return 0;
}
