#include "platform_iwdg_backend.h"
#include "platform_wwdg_backend.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static Platform_StatusTypeDef s_iwdg_start_result;
static Platform_StatusTypeDef s_wwdg_start_result;
static uint32_t s_iwdg_lsi_hz;
static uint32_t s_wwdg_pclk1_hz;
static bool s_iwdg_backend_started;
static bool s_wwdg_backend_started;
static uint32_t s_iwdg_divisor;
static uint32_t s_iwdg_reload;
static uint32_t s_wwdg_divisor;
static uint8_t s_wwdg_window;

uint32_t platform_iwdg_backend_lsi_hz(void) {
  return s_iwdg_lsi_hz;
}

Platform_StatusTypeDef platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
) {
  s_iwdg_divisor = divisor;
  s_iwdg_reload = reload;
  if (s_iwdg_start_result != Platform_OK) {
    return s_iwdg_start_result;
  }
  s_iwdg_backend_started = true;
  return Platform_OK;
}

Platform_StatusTypeDef platform_iwdg_backend_refresh(void) {
  return s_iwdg_backend_started ? Platform_OK : Platform_ERROR;
}

bool platform_iwdg_backend_was_reset(void) {
  return true;
}

uint32_t platform_wwdg_backend_pclk1_hz(void) {
  return s_wwdg_pclk1_hz;
}

Platform_StatusTypeDef platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
) {
  s_wwdg_divisor = divisor;
  s_wwdg_window = window;
  if (s_wwdg_start_result != Platform_OK) {
    return s_wwdg_start_result;
  }
  s_wwdg_backend_started = true;
  return Platform_OK;
}

Platform_StatusTypeDef platform_wwdg_backend_refresh(void) {
  return s_wwdg_backend_started ? Platform_OK : Platform_ERROR;
}

bool platform_wwdg_backend_was_reset(void) {
  return true;
}

static void test_iwdg_contract(void) {
  s_iwdg_lsi_hz = 32000U;
  s_iwdg_start_result = Platform_ERROR;

  assert(Platform_IWDG_Start((Platform_IWDG_Instance)99U, 100U) ==
         Platform_INVALID_ARGUMENT);
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 0U) ==
         Platform_INVALID_ARGUMENT);
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 100U) == Platform_ERROR);
  assert(Platform_IWDG_Refresh(PLATFORM_IWDG_1) == Platform_ERROR);

  s_iwdg_start_result = Platform_OK;
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 100U) == Platform_OK);
  assert(s_iwdg_divisor != 0U);
  assert(s_iwdg_reload != 0U);
  assert(Platform_IWDG_Refresh(PLATFORM_IWDG_1) == Platform_OK);
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 100U) == Platform_BUSY);
  assert(Platform_IWDG_WasReset(PLATFORM_IWDG_1));
  assert(!Platform_IWDG_WasReset((Platform_IWDG_Instance)99U));
}

static void test_wwdg_contract(void) {
  s_wwdg_pclk1_hz = 42000000U;
  assert(Platform_WWDG_GetMaxTimeoutMs(PLATFORM_WWDG_1) == 49U);
  assert(Platform_WWDG_GetMaxTimeoutMs((Platform_WWDG_Instance)99U) == 0U);

  const Platform_WWDG_Config invalid_window = {
    .timeout_ms = 10U,
    .window = PLATFORM_WWDG_WINDOW_MAX + 1U,
  };
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, NULL) ==
         Platform_INVALID_ARGUMENT);
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &invalid_window) ==
         Platform_INVALID_ARGUMENT);

  s_wwdg_start_result = Platform_ERROR;
  const Platform_WWDG_Config config = {
    .timeout_ms = 10U,
    .window = PLATFORM_WWDG_WINDOW_MIN,
  };
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &config) == Platform_ERROR);
  assert(Platform_WWDG_Refresh(PLATFORM_WWDG_1) == Platform_ERROR);

  s_wwdg_start_result = Platform_OK;
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &config) == Platform_OK);
  assert(s_wwdg_divisor != 0U);
  assert(s_wwdg_window == PLATFORM_WWDG_WINDOW_MIN);
  assert(Platform_WWDG_Refresh(PLATFORM_WWDG_1) == Platform_OK);
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &config) == Platform_BUSY);
  assert(Platform_WWDG_WasReset(PLATFORM_WWDG_1));
  assert(!Platform_WWDG_WasReset((Platform_WWDG_Instance)99U));

  s_wwdg_pclk1_hz = 72000000U;
  assert(Platform_WWDG_GetMaxTimeoutMs(PLATFORM_WWDG_1) == 29U);
}

int main(void) {
  test_iwdg_contract();
  test_wwdg_contract();
  return 0;
}
