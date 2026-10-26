/**
 * @file system_record_store_contract_test.c
 * @brief Flash 双页记录存储契约测试。
 * @ingroup tests
 */

#include "platform_flash_backend.h"
#include "platform_flash.h"
#include "system_critical.h"
#include "system_record_store.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define TEST_FLASH_START 0x1000U
#define TEST_FLASH_END   0x11FFU
#define TEST_PAGE_SIZE   0x100U
#define TEST_MAGIC       0x54455354U

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
    size == 0u ||
    address < TEST_FLASH_START ||
    address > TEST_FLASH_END
  ) {
    return false;
  }
  return size <= TEST_FLASH_END - address + 1u;
}

uint32_t platform_flash_backend_erase_unit_size(uint32_t address) {
  return platform_flash_backend_range_valid(address, 1u)
    ? TEST_PAGE_SIZE
    : 0u;
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

static void reset_fake_flash(void) {
  memset(s_flash, 0xFF, sizeof(s_flash));
  s_write_status = PLATFORM_STATUS_OK;
  s_erase_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_FLASH_ERROR_NONE;
}

static void test_append_and_journal_switch(void) {
  reset_fake_flash();
  system_record_store_t store;
  assert(Sys_RecordStore_Init(
    &store,
    PLATFORM_FLASH_1,
    TEST_FLASH_START,
    TEST_PAGE_SIZE,
    TEST_MAGIC
  ) == SYSTEM_RECORD_STORE_OK);

  uint8_t empty_buffer[16];
  uint16_t version = 0u;
  uint32_t size = 0u;
  assert(Sys_RecordStore_LoadLatest(
    &store,
    &version,
    empty_buffer,
    sizeof(empty_buffer),
    &size
  ) == SYSTEM_RECORD_STORE_EMPTY);

  const uint32_t values[3] = {11u, 22u, 33u};
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &values[0],
    sizeof(values[0])
  ) == SYSTEM_RECORD_STORE_OK);
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &values[1],
    sizeof(values[1])
  ) == SYSTEM_RECORD_STORE_OK);
  assert(Sys_RecordStore_Append(
    &store,
    2u,
    &values[2],
    sizeof(values[2])
  ) == SYSTEM_RECORD_STORE_OK);

  uint32_t readback = 0u;
  assert(Sys_RecordStore_LoadLatest(
    &store,
    &version,
    &readback,
    sizeof(readback),
    &size
  ) == SYSTEM_RECORD_STORE_OK);
  assert(version == 2u);
  assert(size == sizeof(readback));
  assert(readback == values[2]);

  assert(Sys_RecordStore_Append(
    &store,
    2u,
    &values[0],
    sizeof(values[0])
  ) == SYSTEM_RECORD_STORE_OK);
  assert(Sys_RecordStore_LoadLatest(
    &store,
    &version,
    &readback,
    sizeof(readback),
    &size
  ) == SYSTEM_RECORD_STORE_OK);
  assert(readback == values[0]);
}

static void test_crc_fallback_and_errors(void) {
  reset_fake_flash();
  system_record_store_t store;
  assert(Sys_RecordStore_Init(
    &store,
    PLATFORM_FLASH_1,
    TEST_FLASH_START,
    TEST_PAGE_SIZE,
    TEST_MAGIC
  ) == SYSTEM_RECORD_STORE_OK);

  const uint32_t first = 100u;
  const uint32_t second = 200u;
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &first,
    sizeof(first)
  ) == SYSTEM_RECORD_STORE_OK);
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &second,
    sizeof(second)
  ) == SYSTEM_RECORD_STORE_OK);

  s_flash[
    SYSTEM_RECORD_STORE_SLOT_SIZE +
    SYSTEM_RECORD_STORE_HEADER_SIZE
  ] ^= 0xFFu;
  uint32_t readback = 0u;
  uint16_t version = 0u;
  uint32_t size = 0u;
  assert(Sys_RecordStore_LoadLatest(
    &store,
    &version,
    &readback,
    sizeof(readback),
    &size
  ) == SYSTEM_RECORD_STORE_OK);
  assert(readback == first);

  assert(Sys_RecordStore_LoadLatest(
    &store,
    &version,
    &readback,
    sizeof(uint8_t),
    &size
  ) == SYSTEM_RECORD_STORE_BUFFER_TOO_SMALL);

  s_write_status = PLATFORM_STATUS_ERROR;
  uint32_t third = 300u;
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &third,
    sizeof(third)
  ) == SYSTEM_RECORD_STORE_FLASH_ERROR);

  s_write_status = PLATFORM_STATUS_OK;
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &third,
    sizeof(third)
  ) == SYSTEM_RECORD_STORE_OK);
  assert(Sys_RecordStore_LoadLatest(
    &store,
    &version,
    &readback,
    sizeof(readback),
    &size
  ) == SYSTEM_RECORD_STORE_OK);
  assert(readback == third);
}

static void test_init_erase_failure_is_atomic(void) {
  reset_fake_flash();
  s_erase_status = PLATFORM_STATUS_ERROR;

  system_record_store_t store;
  assert(Sys_RecordStore_Init(
    &store,
    PLATFORM_FLASH_1,
    TEST_FLASH_START,
    TEST_PAGE_SIZE,
    TEST_MAGIC
  ) == SYSTEM_RECORD_STORE_FLASH_ERROR);

  uint32_t payload = 1u;
  assert(Sys_RecordStore_Append(
    &store,
    1u,
    &payload,
    sizeof(payload)
  ) == SYSTEM_RECORD_STORE_INVALID_ARGUMENT);
}

int main(void) {
  test_append_and_journal_switch();
  test_crc_fallback_and_errors();
  test_init_erase_failure_is_atomic();
  return 0;
}
