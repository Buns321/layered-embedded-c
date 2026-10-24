/**
 * @file flash_contract_test.c
 * @brief Flash Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_flash_backend.h"
#include "platform_flash.h"
#include "system_critical.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TEST_FLASH_START 0x1000U
#define TEST_FLASH_END   0x1FFFU
#define TEST_ERASE_UNIT  256U

static uint8_t s_flash[TEST_FLASH_END - TEST_FLASH_START + 1U];
static platform_status_t s_write_status;
static platform_status_t s_erase_status;
static uint32_t s_backend_error;

void Sys_Critical_Enter(void) {
}

void Sys_Critical_Exit(void) {
}

bool platform_flash_backend_range_valid(uint32_t address, uint32_t size) {
  if (
    size == 0U ||
    address < TEST_FLASH_START ||
    address > TEST_FLASH_END
  ) {
    return false;
  }
  return size <= TEST_FLASH_END - address + 1U;
}

uint32_t platform_flash_backend_erase_unit_size(uint32_t address) {
  return platform_flash_backend_range_valid(address, 1U)
           ? TEST_ERASE_UNIT
           : 0U;
}

platform_status_t platform_flash_backend_read(
  uint32_t address,
  void *data,
  uint32_t size
) {
  memcpy(data, &s_flash[address - TEST_FLASH_START], size);
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_flash_backend_write(
  uint32_t address,
  const void *data,
  uint32_t size
) {
  if (s_write_status != PLATFORM_STATUS_OK) {
    s_backend_error = PLATFORM_FLASH_ERROR_PROGRAM;
    return s_write_status;
  }
  memcpy(&s_flash[address - TEST_FLASH_START], data, size);
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_flash_backend_erase(
  uint32_t address,
  uint32_t size
) {
  if (s_erase_status != PLATFORM_STATUS_OK) {
    s_backend_error = PLATFORM_FLASH_ERROR_ERASE;
    return s_erase_status;
  }
  memset(&s_flash[address - TEST_FLASH_START], 0xFF, size);
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_flash_backend_get_error(void) {
  return s_backend_error;
}

static void test_flash_contract(void) {
  s_write_status = PLATFORM_STATUS_OK;
  s_erase_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;

  const uint32_t words[2] = {0x11223344U, 0x55667788U};
  assert(Platform_FLASH_Write(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           words,
           sizeof(words)
         ) == PLATFORM_STATUS_OK);

  uint32_t readback[2] = {0};
  assert(Platform_FLASH_Read(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           readback,
           sizeof(readback)
         ) == PLATFORM_STATUS_OK);
  assert(readback[0] == words[0]);
  assert(readback[1] == words[1]);

  assert(Platform_FLASH_Write(
           PLATFORM_FLASH_1,
           TEST_FLASH_START + 1U,
           words,
           sizeof(words)
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_FLASH_ERROR_ALIGNMENT) != 0U);

  assert(Platform_FLASH_Erase(
           PLATFORM_FLASH_1,
           TEST_FLASH_START + 1U,
           TEST_ERASE_UNIT
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_FLASH_ERROR_ALIGNMENT) != 0U);

  assert(Platform_FLASH_Erase(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           TEST_ERASE_UNIT
         ) == PLATFORM_STATUS_OK);
  assert(Platform_FLASH_GetLastError(PLATFORM_FLASH_1) ==
         PLATFORM_ERROR_NONE);
  assert(Platform_FLASH_GetEraseUnitSize(
           PLATFORM_FLASH_1,
           TEST_FLASH_START
         ) == TEST_ERASE_UNIT);
  assert(Platform_FLASH_GetEraseUnitSize(
           PLATFORM_FLASH_1,
           TEST_FLASH_END + 1U
         ) == 0U);

  s_write_status = PLATFORM_STATUS_ERROR;
  s_backend_error = PLATFORM_FLASH_ERROR_PROGRAM;
  assert(Platform_FLASH_Write(
           PLATFORM_FLASH_1,
           TEST_FLASH_START,
           words,
           sizeof(words)
         ) == PLATFORM_STATUS_ERROR);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_FLASH_ERROR_PROGRAM) != 0U);
  assert((Platform_FLASH_GetLastError(PLATFORM_FLASH_1) &
          PLATFORM_ERROR_HAL) != 0U);

  assert((Platform_FLASH_GetLastError((platform_flash_instance_t)99U) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

int main(void) {
  test_flash_contract();
  return 0;
}
