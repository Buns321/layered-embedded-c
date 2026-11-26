/**
 * @file test_sys_record_store.c
 * @brief 双页 CRC32 持久记录存储宿主机单元测试。
 * @ingroup tests
 */

#include "bsp_def.h"
#include "bsp_mcu.h"
#include "sys_record_store.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TEST_FLASH_BASE 0x1000U
#define TEST_PAGE_SIZE_BYTES 0x100U
#define TEST_FLASH_TOTAL (TEST_PAGE_SIZE_BYTES * 2U)
#define TEST_MAGIC 0x54455354U

static void setup_flash(void) {
  BSP_Stub_ResetAll();
  BSP_Stub_FlashConfig(TEST_FLASH_BASE, TEST_FLASH_TOTAL, TEST_PAGE_SIZE_BYTES);
}

static void test_append_bank_switch_and_reboot_recovery(void) {
  setup_flash();
  sys_record_store_t store;
  assert(Sys_RecordStore_Init(&store, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_OK);

  uint8_t empty_buf[16];
  uint16_t version = 0U;
  uint32_t size = 0U;
  assert(Sys_RecordStore_LoadLatest(&store, &version, empty_buf, sizeof(empty_buf), &size) ==
         SYS_RECORD_STORE_EMPTY);

  /* 每页 2 个 128 byte 槽位，第 3 条触发页切换擦除第 1 页 */
  const uint32_t values[4] = {11U, 22U, 33U, 44U};
  assert(Sys_RecordStore_Append(&store, 1U, &values[0], sizeof(values[0])) == SYS_RECORD_STORE_OK);
  assert(Sys_RecordStore_Append(&store, 1U, &values[1], sizeof(values[1])) == SYS_RECORD_STORE_OK);
  assert(Sys_RecordStore_Append(&store, 2U, &values[2], sizeof(values[2])) == SYS_RECORD_STORE_OK);

  uint32_t readback = 0U;
  assert(Sys_RecordStore_LoadLatest(&store, &version, &readback, sizeof(readback), &size) ==
         SYS_RECORD_STORE_OK);
  assert(version == 2U && size == sizeof(readback) && readback == values[2]);

  assert(Sys_RecordStore_Append(&store, 3U, &values[3], sizeof(values[3])) == SYS_RECORD_STORE_OK);

  /* 重新初始化模拟掉电重启恢复 */
  sys_record_store_t rebooted;
  assert(Sys_RecordStore_Init(&rebooted, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_OK);
  assert(rebooted.active_bank == 1U);
  assert(rebooted.next_slot == 2U);
  assert(rebooted.next_sequence == 5U);
  readback = 0U;
  assert(Sys_RecordStore_LoadLatest(&rebooted, &version, &readback, sizeof(readback), &size) ==
         SYS_RECORD_STORE_OK);
  assert(version == 3U && readback == values[3]);
}

static void test_crc_fallback_and_bad_slot_recovery(void) {
  setup_flash();
  sys_record_store_t store;
  assert(Sys_RecordStore_Init(&store, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_OK);

  const uint32_t first = 100U;
  const uint32_t second = 200U;
  assert(Sys_RecordStore_Append(&store, 1U, &first, sizeof(first)) == SYS_RECORD_STORE_OK);
  assert(Sys_RecordStore_Append(&store, 1U, &second, sizeof(second)) == SYS_RECORD_STORE_OK);

  /* 破坏第二条记录载荷，验证回退到第一条记录 */
  uint8_t *flash_mem = BSP_Stub_FlashBuffer();
  flash_mem[SYS_RECORD_STORE_SLOT_SIZE + SYS_RECORD_STORE_HEADER_SIZE] ^= 0xFFU;

  uint32_t readback = 0U;
  uint16_t version = 0U;
  uint32_t size = 0U;
  assert(Sys_RecordStore_LoadLatest(&store, &version, &readback, sizeof(readback), &size) ==
         SYS_RECORD_STORE_OK);
  assert(readback == first);
  assert(Sys_RecordStore_LoadLatest(&store, &version, &readback, sizeof(uint8_t), &size) ==
         SYS_RECORD_STORE_BUFFER_TOO_SMALL);

  /* 注入写入失败与回读校验失败，验证坏槽跳过且不浪费序号 */
  BSP_Stub_FlashSetWriteStatus(BSP_ERR_HAL);
  uint32_t third = 300U;
  assert(Sys_RecordStore_Append(&store, 1U, &third, sizeof(third)) == SYS_RECORD_STORE_FLASH_ERROR);
  BSP_Stub_FlashSetWriteStatus(BSP_OK);

  BSP_Stub_FlashSetCorruptNextWrite(true);
  assert(Sys_RecordStore_Append(&store, 1U, &third, sizeof(third)) == SYS_RECORD_STORE_FLASH_ERROR);

  assert(Sys_RecordStore_Append(&store, 2U, &third, sizeof(third)) == SYS_RECORD_STORE_OK);
  assert(Sys_RecordStore_LoadLatest(&store, &version, &readback, sizeof(readback), &size) ==
         SYS_RECORD_STORE_OK);
  assert(version == 2U && readback == third);
}

static void test_sequence_wrap_and_validation(void) {
  setup_flash();
  sys_record_store_t store;
  assert(Sys_RecordStore_Init(&store, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_OK);

  store.next_sequence = UINT32_MAX;
  const uint32_t v1 = 0xAAU;
  const uint32_t v2 = 0xBBU;
  assert(Sys_RecordStore_Append(&store, 1U, &v1, sizeof(v1)) == SYS_RECORD_STORE_OK);
  assert(store.next_sequence == 1U);
  assert(Sys_RecordStore_Append(&store, 2U, &v2, sizeof(v2)) == SYS_RECORD_STORE_OK);

  uint32_t readback = 0U;
  uint16_t version = 0U;
  uint32_t size = 0U;
  assert(Sys_RecordStore_LoadLatest(&store, &version, &readback, sizeof(readback), &size) ==
         SYS_RECORD_STORE_OK);
  assert(version == 2U && readback == v2);

  /* 非法参数与初始化擦除失败原子性 */
  assert(Sys_RecordStore_Init(NULL, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_INVALID_ARG);
  assert(Sys_RecordStore_Init(&store, TEST_FLASH_BASE + 4U, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_INVALID_ARG);
  assert(Sys_RecordStore_Init(&store, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, 0xFFFFFFFFU) ==
         SYS_RECORD_STORE_INVALID_ARG);
  assert(Sys_RecordStore_Init(&store, UINT32_MAX - 10U, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_INVALID_ARG);

  setup_flash();
  BSP_Stub_FlashSetEraseStatus(BSP_ERR_HAL);
  assert(Sys_RecordStore_Init(&store, TEST_FLASH_BASE, TEST_PAGE_SIZE_BYTES, TEST_MAGIC) ==
         SYS_RECORD_STORE_FLASH_ERROR);
  assert(Sys_RecordStore_Append(&store, 1U, &v1, sizeof(v1)) == SYS_RECORD_STORE_INVALID_ARG);
}

int main(void) {
  test_append_bank_switch_and_reboot_recovery();
  test_crc_fallback_and_bad_slot_recovery();
  test_sequence_wrap_and_validation();
  (void)puts("test_sys_record_store passed");
  return 0;
}
