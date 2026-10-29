//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file system_record_store.c
 * @brief 基于内部 Flash 的双页追加记录存储实现。
 * @ingroup system
 */

#include "system_record_store.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define SYSTEM_RECORD_STORE_EMPTY_MARKER 0xFFFFFFFFU
#define SYSTEM_RECORD_STORE_CRC_POLYNOMIAL 0xEDB88320U

typedef struct system_record_store_header_s {
  uint32_t magic;
  uint16_t version;
  uint16_t payload_size;
  uint32_t sequence;
  uint32_t payload_crc;
  uint32_t header_crc;
} system_record_store_header_t;

typedef struct system_record_store_location_s {
  uint32_t sequence;
  uint16_t slot;
  uint8_t bank;
  bool found;
} system_record_store_location_t;

_Static_assert(
  sizeof(system_record_store_header_t) == SYSTEM_RECORD_STORE_HEADER_SIZE,
  "record store header layout changed"
);
_Static_assert(
  SYSTEM_RECORD_STORE_SLOT_SIZE % 4u == 0u,
  "record store slot must be word aligned"
);
_Static_assert(
  SYSTEM_RECORD_STORE_HEADER_SIZE % 4u == 0u,
  "record store header must be word aligned"
);

/**
 * @brief 按字节更新记录 CRC32 状态。
 */
static uint32_t sys_recordstore_crc32update(
  uint32_t crc,
  const void *data,
  uint32_t size
) {
  const uint8_t *bytes = (const uint8_t *)data;

  for (uint32_t index = 0u; index < size; index++) {
    crc ^= bytes[index];
    for (uint8_t bit = 0u; bit < 8u; bit++) {
      uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
      crc = (crc >> 1u) ^
        (SYSTEM_RECORD_STORE_CRC_POLYNOMIAL & mask);
    }
  }

  return crc;
}

/**
 * @brief 计算记录头与载荷的 CRC。
 */
static uint32_t sys_recordstore_recordcrc(
  const system_record_store_header_t *header,
  const uint8_t *payload
) {
  uint32_t crc = 0xFFFFFFFFU;
  crc = sys_recordstore_crc32update(
    crc,
    header,
    offsetof(system_record_store_header_t, header_crc)
  );
  crc = sys_recordstore_crc32update(crc, payload, header->payload_size);
  return ~crc;
}

/**
 * @brief 计算槽位在 Flash 中的地址。
 */
static uint32_t sys_recordstore_slotaddress(
  const system_record_store_t *store_h,
  uint8_t bank,
  uint16_t slot
) {
  return store_h->bank_address[bank] +
    ((uint32_t)slot * SYSTEM_RECORD_STORE_SLOT_SIZE);
}

/**
 * @brief 判断记录槽头是否为空。
 */
static bool sys_recordstore_headerisempty(
  const system_record_store_header_t *header
) {
  return
    header->magic == SYSTEM_RECORD_STORE_EMPTY_MARKER &&
    header->sequence == SYSTEM_RECORD_STORE_EMPTY_MARKER;
}

/**
 * @brief 判断记录槽头是否有效。
 */
static bool sys_recordstore_headerisvalid(
  const system_record_store_t *store_h,
  const system_record_store_header_t *header,
  const uint8_t *slot
) {
  if (
    header->magic != store_h->magic ||
    header->payload_size == 0u ||
    header->payload_size > SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY
  ) {
    return false;
  }

  const uint8_t *payload = slot + SYSTEM_RECORD_STORE_HEADER_SIZE;
  uint32_t header_crc = sys_recordstore_recordcrc(header, payload);
  return header_crc == header->header_crc;
}

/**
 * @brief 扫描两页记录槽并找到最新有效记录。
 */
static system_record_store_status_t sys_recordstore_scan(
  const system_record_store_t *store_h,
  system_record_store_location_t *location
) {
  system_record_store_location_t latest = {
    .found = false,
  };

  for (uint8_t bank = 0u; bank < SYSTEM_RECORD_STORE_BANK_COUNT; bank++) {
    for (uint16_t slot = 0u; slot < store_h->slot_count; slot++) {
      uint32_t address = sys_recordstore_slotaddress(store_h, bank, slot);
      uint8_t slot_data[SYSTEM_RECORD_STORE_SLOT_SIZE];

      if (
        Platform_FLASH_Read(
          store_h->flash,
          address,
          slot_data,
          sizeof(slot_data)
        ) != PLATFORM_STATUS_OK
      ) {
        return SYSTEM_RECORD_STORE_FLASH_ERROR;
      }

      system_record_store_header_t header;
      memcpy(&header, slot_data, sizeof(header));
      if (sys_recordstore_headerisempty(&header)) {
        continue;
      }
      if (!sys_recordstore_headerisvalid(store_h, &header, slot_data)) {
        continue;
      }

      if (
        !latest.found ||
        (int32_t)(header.sequence - latest.sequence) > 0
      ) {
        latest.found = true;
        latest.sequence = header.sequence;
        latest.slot = slot;
        latest.bank = bank;
      }
    }
  }

  *location = latest;
  return latest.found
    ? SYSTEM_RECORD_STORE_OK
    : SYSTEM_RECORD_STORE_EMPTY;
}

/**
 * @brief 擦除记录存储页。
 */
static system_record_store_status_t sys_recordstore_erasebank(
  system_record_store_t *store_h,
  uint8_t bank
) {
  if (
    Platform_FLASH_Erase(
      store_h->flash,
      store_h->bank_address[bank],
      store_h->page_size
    ) != PLATFORM_STATUS_OK
  ) {
    return SYSTEM_RECORD_STORE_FLASH_ERROR;
  }
  return SYSTEM_RECORD_STORE_OK;
}

/**
 * @brief 把损坏槽位标记为跳过。
 */
static void sys_recordstore_skipfailedslot(system_record_store_t *store_h) {
  assert(store_h);

  if (store_h->next_slot < store_h->slot_count) {
    store_h->next_slot++;
  }
}

/**
 * @brief 初始化双页记录存储并扫描最新有效记录。
 * @param store_h 对象。
 * @param flash 内部 Flash 实例。
 * @param first_bank_address 第一页起始地址，必须按擦除单元对齐。
 * @param page_size 单页大小，必须等于实际擦除单元且容纳至少一个记录。
 * @param magic 应用记录 Magic，不能为全 1。
 * @retval SYSTEM_RECORD_STORE_OK 初始化成功。
 * @retval SYSTEM_RECORD_STORE_INVALID_ARGUMENT 参数或 Flash 范围非法。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR 扫描或初始化擦除失败。
 */
system_record_store_status_t Sys_RecordStore_Init(
  system_record_store_t *store_h,
  platform_flash_instance_t flash,
  uint32_t first_bank_address,
  uint32_t page_size,
  uint32_t magic
) {
  if (
    store_h == NULL ||
    page_size < SYSTEM_RECORD_STORE_SLOT_SIZE ||
    (page_size % SYSTEM_RECORD_STORE_SLOT_SIZE) != 0u ||
    magic == SYSTEM_RECORD_STORE_EMPTY_MARKER
  ) {
    return SYSTEM_RECORD_STORE_INVALID_ARGUMENT;
  }

  memset(store_h, 0, sizeof(*store_h));
  store_h->flash = flash;
  store_h->page_size = page_size;
  store_h->magic = magic;
  store_h->slot_count = (uint16_t)(
    page_size / SYSTEM_RECORD_STORE_SLOT_SIZE
  );

  for (uint8_t bank = 0u; bank < SYSTEM_RECORD_STORE_BANK_COUNT; bank++) {
    store_h->bank_address[bank] =
      first_bank_address + ((uint32_t)bank * page_size);
  }

  const uint32_t erase_size_0 =
    Platform_FLASH_GetEraseUnitSize(flash, store_h->bank_address[0]);
  const uint32_t erase_size_1 =
    Platform_FLASH_GetEraseUnitSize(flash, store_h->bank_address[1]);
  if (
    erase_size_0 != page_size ||
    erase_size_1 != page_size ||
    (first_bank_address % page_size) != 0u
  ) {
    return SYSTEM_RECORD_STORE_INVALID_ARGUMENT;
  }

  system_record_store_location_t latest;
  system_record_store_status_t scan_status =
    sys_recordstore_scan(store_h, &latest);
  if (scan_status == SYSTEM_RECORD_STORE_FLASH_ERROR) {
    return scan_status;
  }

  if (!latest.found) {
    store_h->active_bank = 0u;
    store_h->next_slot = 0u;
    store_h->next_sequence = 1u;
    system_record_store_status_t erase_status =
      sys_recordstore_erasebank(store_h, 0u);
    if (erase_status != SYSTEM_RECORD_STORE_OK) {
      return erase_status;
    }
    store_h->initialized = true;
    return SYSTEM_RECORD_STORE_OK;
  }

  store_h->active_bank = latest.bank;
  store_h->next_slot = (uint16_t)(latest.slot + 1u);
  store_h->next_sequence =
    latest.sequence == UINT32_MAX ? 1u : latest.sequence + 1u;
  store_h->initialized = true;
  return SYSTEM_RECORD_STORE_OK;
}

/**
 * @brief 读取序号最新的有效记录。
 * @param store_h 已初始化对象。
 * @param version 输出记录版本。
 * @param payload 输出负载缓冲区。
 * @param payload_capacity 缓冲区容量（byte）。
 * @param payload_size 输出实际负载长度（byte）。
 * @retval SYSTEM_RECORD_STORE_OK 找到并复制记录。
 * @retval SYSTEM_RECORD_STORE_EMPTY 没有有效记录。
 * @retval SYSTEM_RECORD_STORE_BUFFER_TOO_SMALL 缓冲区不足。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR Flash 读取失败。
 * @note payload 指向调用方拥有的输出缓冲区；本函数只在返回前写入。
 */
system_record_store_status_t Sys_RecordStore_LoadLatest(
  const system_record_store_t *store_h,
  uint16_t *version,
  void *payload,
  uint32_t payload_capacity,
  uint32_t *payload_size
) {
  if (
    store_h == NULL ||
    !store_h->initialized ||
    version == NULL ||
    payload == NULL ||
    payload_size == NULL
  ) {
    return SYSTEM_RECORD_STORE_INVALID_ARGUMENT;
  }

  system_record_store_location_t latest;
  system_record_store_status_t scan_status =
    sys_recordstore_scan(store_h, &latest);
  if (scan_status != SYSTEM_RECORD_STORE_OK) {
    return scan_status;
  }

  uint8_t slot_data[SYSTEM_RECORD_STORE_SLOT_SIZE];
  if (
    Platform_FLASH_Read(
      store_h->flash,
      sys_recordstore_slotaddress(
        store_h,
        latest.bank,
        latest.slot
      ),
      slot_data,
      sizeof(slot_data)
    ) != PLATFORM_STATUS_OK
  ) {
    return SYSTEM_RECORD_STORE_FLASH_ERROR;
  }

  system_record_store_header_t header;
  memcpy(&header, slot_data, sizeof(header));
  if (!sys_recordstore_headerisvalid(store_h, &header, slot_data)) {
    return SYSTEM_RECORD_STORE_INVALID_RECORD;
  }
  if (header.payload_size > payload_capacity) {
    return SYSTEM_RECORD_STORE_BUFFER_TOO_SMALL;
  }

  memcpy(
    payload,
    slot_data + SYSTEM_RECORD_STORE_HEADER_SIZE,
    header.payload_size
  );
  *version = header.version;
  *payload_size = header.payload_size;
  return SYSTEM_RECORD_STORE_OK;
}

/**
 * @brief 追加一条记录。
 * @param store_h 已初始化对象。
 * @param version 应用记录版本。
 * @param payload 负载数据。
 * @param payload_size 负载长度，范围 1..SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY。
 * @retval SYSTEM_RECORD_STORE_OK 写入和回读校验成功。
 * @retval SYSTEM_RECORD_STORE_INVALID_ARGUMENT 参数非法。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR Flash 擦除、写入或校验失败。
 * @note 只允许单一写者从任务上下文调用。
 * @note payload 在调用期间只读，不需要保持到函数返回之后。
 */
system_record_store_status_t Sys_RecordStore_Append(
  system_record_store_t *store_h,
  uint16_t version,
  const void *payload,
  uint32_t payload_size
) {
  if (
    store_h == NULL ||
    !store_h->initialized ||
    payload == NULL ||
    payload_size == 0u ||
    payload_size > SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY
  ) {
    return SYSTEM_RECORD_STORE_INVALID_ARGUMENT;
  }

  if (store_h->next_slot >= store_h->slot_count) {
    uint8_t next_bank = (uint8_t)(store_h->active_bank ^ 1u);
    system_record_store_status_t erase_status =
      sys_recordstore_erasebank(store_h, next_bank);
    if (erase_status != SYSTEM_RECORD_STORE_OK) {
      return erase_status;
    }
    store_h->active_bank = next_bank;
    store_h->next_slot = 0u;
  }

  memset(store_h->slot_buffer, 0, sizeof(store_h->slot_buffer));

  system_record_store_header_t header = {
    .magic = store_h->magic,
    .version = version,
    .payload_size = (uint16_t)payload_size,
    .sequence = store_h->next_sequence,
  };
  uint8_t *slot_payload =
    store_h->slot_buffer + SYSTEM_RECORD_STORE_HEADER_SIZE;
  memcpy(slot_payload, payload, payload_size);
  header.header_crc = sys_recordstore_recordcrc(&header, slot_payload);
  memcpy(store_h->slot_buffer, &header, sizeof(header));

  uint32_t address = sys_recordstore_slotaddress(
    store_h,
    store_h->active_bank,
    store_h->next_slot
  );
  if (
    Platform_FLASH_Write(
      store_h->flash,
      address,
      store_h->slot_buffer,
      sizeof(store_h->slot_buffer)
    ) != PLATFORM_STATUS_OK
  ) {
    sys_recordstore_skipfailedslot(store_h);
    return SYSTEM_RECORD_STORE_FLASH_ERROR;
  }

  uint8_t verify_buffer[SYSTEM_RECORD_STORE_SLOT_SIZE];
  if (
    Platform_FLASH_Read(
      store_h->flash,
      address,
      verify_buffer,
      sizeof(verify_buffer)
    ) != PLATFORM_STATUS_OK ||
    memcmp(
      verify_buffer,
      store_h->slot_buffer,
      sizeof(verify_buffer)
    ) != 0
  ) {
    sys_recordstore_skipfailedslot(store_h);
    return SYSTEM_RECORD_STORE_FLASH_ERROR;
  }

  store_h->next_slot++;
  store_h->next_sequence =
    store_h->next_sequence == UINT32_MAX
      ? 1u
      : store_h->next_sequence + 1u;
  return SYSTEM_RECORD_STORE_OK;
}
