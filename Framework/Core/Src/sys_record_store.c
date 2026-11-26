/**
 * @file sys_record_store.c
 * @brief 基于 bsp_flash 的双页 CRC32 记录存储实现。
 * @ingroup sys_core
 */

#include "sys_record_store.h"
#include "bsp_flash.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define SYS_RECORD_STORE_EMPTY_MARKER 0xFFFFFFFFU
#define SYS_RECORD_STORE_CRC_POLYNOMIAL 0xEDB88320U

typedef struct sys_record_store_header_s {
  uint32_t magic;
  uint16_t version;
  uint16_t payload_size;
  uint32_t sequence;
  uint32_t payload_crc;
  uint32_t header_crc;
} sys_record_store_header_t;

typedef struct sys_record_store_location_s {
  uint32_t sequence;
  uint16_t slot;
  uint8_t bank;
  bool found;
} sys_record_store_location_t;

_Static_assert(sizeof(sys_record_store_header_t) == SYS_RECORD_STORE_HEADER_SIZE,
               "record store header layout changed");
_Static_assert(SYS_RECORD_STORE_SLOT_SIZE % 4U == 0U, "record store slot must be word aligned");
_Static_assert(SYS_RECORD_STORE_HEADER_SIZE % 4U == 0U, "record store header must be word aligned");

static uint32_t sys_recordstore_crc32update(uint32_t crc, const void *data, uint32_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  for (uint32_t index = 0U; index < size; index++) {
    crc ^= bytes[index];
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (SYS_RECORD_STORE_CRC_POLYNOMIAL & mask);
    }
  }
  return crc;
}

static uint32_t sys_recordstore_recordcrc(const sys_record_store_header_t *header,
                                          const uint8_t *payload) {
  uint32_t crc = 0xFFFFFFFFU;
  crc = sys_recordstore_crc32update(crc, header, offsetof(sys_record_store_header_t, header_crc));
  crc = sys_recordstore_crc32update(crc, payload, header->payload_size);
  return ~crc;
}

static uint32_t sys_recordstore_slotaddress(const sys_record_store_t *store_h, uint8_t bank,
                                            uint16_t slot) {
  return store_h->bank_address[bank] + ((uint32_t)slot * SYS_RECORD_STORE_SLOT_SIZE);
}

static bool sys_recordstore_headerisempty(const sys_record_store_header_t *header) {
  return header->magic == SYS_RECORD_STORE_EMPTY_MARKER &&
         header->sequence == SYS_RECORD_STORE_EMPTY_MARKER;
}

static bool sys_recordstore_headerisvalid(const sys_record_store_t *store_h,
                                          const sys_record_store_header_t *header,
                                          const uint8_t *slot) {
  if (header->magic != store_h->magic || header->payload_size == 0U ||
      header->payload_size > SYS_RECORD_STORE_PAYLOAD_CAPACITY) {
    return false;
  }

  const uint8_t *payload = slot + SYS_RECORD_STORE_HEADER_SIZE;
  return sys_recordstore_recordcrc(header, payload) == header->header_crc;
}

static sys_record_store_status_t sys_recordstore_scan(const sys_record_store_t *store_h,
                                                      sys_record_store_location_t *location) {
  sys_record_store_location_t latest = {
      .found = false,
  };

  for (uint8_t bank = 0U; bank < SYS_RECORD_STORE_BANK_COUNT; bank++) {
    for (uint16_t slot = 0U; slot < store_h->slot_count; slot++) {
      uint32_t address = sys_recordstore_slotaddress(store_h, bank, slot);
      uint8_t slot_data[SYS_RECORD_STORE_SLOT_SIZE];

      if (BSP_Flash_Read(address, slot_data, sizeof(slot_data)) != BSP_OK) {
        return SYS_RECORD_STORE_FLASH_ERROR;
      }

      sys_record_store_header_t header;
      (void)memcpy(&header, slot_data, sizeof(header));
      if (sys_recordstore_headerisempty(&header)) {
        continue;
      }
      if (!sys_recordstore_headerisvalid(store_h, &header, slot_data)) {
        continue;
      }

      if (!latest.found || (int32_t)(header.sequence - latest.sequence) > 0) {
        latest.found = true;
        latest.sequence = header.sequence;
        latest.slot = slot;
        latest.bank = bank;
      }
    }
  }

  *location = latest;
  return latest.found ? SYS_RECORD_STORE_OK : SYS_RECORD_STORE_EMPTY;
}

static sys_record_store_status_t sys_recordstore_erasebank(sys_record_store_t *store_h,
                                                           uint8_t bank) {
  if (BSP_Flash_Erase(store_h->bank_address[bank], store_h->page_size_bytes) != BSP_OK) {
    return SYS_RECORD_STORE_FLASH_ERROR;
  }
  return SYS_RECORD_STORE_OK;
}

static void sys_recordstore_skipfailedslot(sys_record_store_t *store_h) {
  assert(store_h != NULL);
  if (store_h->next_slot < store_h->slot_count) {
    store_h->next_slot++;
  }
}

sys_record_store_status_t Sys_RecordStore_Init(sys_record_store_t *store_h,
                                               uint32_t first_bank_address,
                                               uint32_t page_size_bytes, uint32_t magic) {
  if (store_h == NULL || page_size_bytes < SYS_RECORD_STORE_SLOT_SIZE ||
      (page_size_bytes % SYS_RECORD_STORE_SLOT_SIZE) != 0U ||
      (page_size_bytes / SYS_RECORD_STORE_SLOT_SIZE) > UINT16_MAX ||
      magic == SYS_RECORD_STORE_EMPTY_MARKER) {
    return SYS_RECORD_STORE_INVALID_ARG;
  }

  if (first_bank_address > UINT32_MAX - page_size_bytes) {
    return SYS_RECORD_STORE_INVALID_ARG;
  }
  uint32_t second_bank_address = first_bank_address + page_size_bytes;
  if (second_bank_address > UINT32_MAX - (page_size_bytes - 1U)) {
    return SYS_RECORD_STORE_INVALID_ARG;
  }

  (void)memset(store_h, 0, sizeof(*store_h));
  store_h->page_size_bytes = page_size_bytes;
  store_h->magic = magic;
  store_h->slot_count = (uint16_t)(page_size_bytes / SYS_RECORD_STORE_SLOT_SIZE);
  store_h->bank_address[0] = first_bank_address;
  store_h->bank_address[1] = second_bank_address;

  uint32_t erase_size_0 = BSP_Flash_GetEraseUnitSize(store_h->bank_address[0]);
  uint32_t erase_size_1 = BSP_Flash_GetEraseUnitSize(store_h->bank_address[1]);
  if (erase_size_0 != page_size_bytes || erase_size_1 != page_size_bytes ||
      (first_bank_address % page_size_bytes) != 0U) {
    return SYS_RECORD_STORE_INVALID_ARG;
  }

  sys_record_store_location_t latest;
  sys_record_store_status_t scan_status = sys_recordstore_scan(store_h, &latest);
  if (scan_status == SYS_RECORD_STORE_FLASH_ERROR) {
    return scan_status;
  }

  if (!latest.found) {
    store_h->active_bank = 0U;
    store_h->next_slot = 0U;
    store_h->next_sequence = 1U;
    sys_record_store_status_t erase_status = sys_recordstore_erasebank(store_h, 0U);
    if (erase_status != SYS_RECORD_STORE_OK) {
      return erase_status;
    }
    store_h->initialized = true;
    return SYS_RECORD_STORE_OK;
  }

  store_h->active_bank = latest.bank;
  store_h->next_slot = (uint16_t)(latest.slot + 1U);
  store_h->next_sequence = (latest.sequence == UINT32_MAX) ? 1U : (latest.sequence + 1U);
  store_h->initialized = true;
  return SYS_RECORD_STORE_OK;
}

sys_record_store_status_t Sys_RecordStore_LoadLatest(const sys_record_store_t *store_h,
                                                     uint16_t *version, void *payload,
                                                     uint32_t payload_capacity_bytes,
                                                     uint32_t *payload_size_bytes) {
  if (store_h == NULL || !store_h->initialized || version == NULL || payload == NULL ||
      payload_size_bytes == NULL) {
    return SYS_RECORD_STORE_INVALID_ARG;
  }

  sys_record_store_location_t latest;
  sys_record_store_status_t scan_status = sys_recordstore_scan(store_h, &latest);
  if (scan_status != SYS_RECORD_STORE_OK) {
    return scan_status;
  }

  uint8_t slot_data[SYS_RECORD_STORE_SLOT_SIZE];
  uint32_t address = sys_recordstore_slotaddress(store_h, latest.bank, latest.slot);
  if (BSP_Flash_Read(address, slot_data, sizeof(slot_data)) != BSP_OK) {
    return SYS_RECORD_STORE_FLASH_ERROR;
  }

  sys_record_store_header_t header;
  (void)memcpy(&header, slot_data, sizeof(header));
  if (!sys_recordstore_headerisvalid(store_h, &header, slot_data)) {
    return SYS_RECORD_STORE_INVALID_RECORD;
  }
  if (header.payload_size > payload_capacity_bytes) {
    return SYS_RECORD_STORE_BUFFER_TOO_SMALL;
  }

  (void)memcpy(payload, slot_data + SYS_RECORD_STORE_HEADER_SIZE, header.payload_size);
  *version = header.version;
  *payload_size_bytes = header.payload_size;
  return SYS_RECORD_STORE_OK;
}

sys_record_store_status_t Sys_RecordStore_Append(sys_record_store_t *store_h, uint16_t version,
                                                 const void *payload, uint32_t payload_size_bytes) {
  if (store_h == NULL || !store_h->initialized || payload == NULL || payload_size_bytes == 0U ||
      payload_size_bytes > SYS_RECORD_STORE_PAYLOAD_CAPACITY) {
    return SYS_RECORD_STORE_INVALID_ARG;
  }

  if (store_h->next_slot >= store_h->slot_count) {
    uint8_t next_bank = (uint8_t)(store_h->active_bank ^ 1U);
    sys_record_store_status_t erase_status = sys_recordstore_erasebank(store_h, next_bank);
    if (erase_status != SYS_RECORD_STORE_OK) {
      return erase_status;
    }
    store_h->active_bank = next_bank;
    store_h->next_slot = 0U;
  }

  (void)memset(store_h->slot_buffer, 0, sizeof(store_h->slot_buffer));

  sys_record_store_header_t header = {
      .magic = store_h->magic,
      .version = version,
      .payload_size = (uint16_t)payload_size_bytes,
      .sequence = store_h->next_sequence,
  };
  uint8_t *slot_payload = store_h->slot_buffer + SYS_RECORD_STORE_HEADER_SIZE;
  (void)memcpy(slot_payload, payload, payload_size_bytes);
  header.header_crc = sys_recordstore_recordcrc(&header, slot_payload);
  (void)memcpy(store_h->slot_buffer, &header, sizeof(header));

  uint32_t address = sys_recordstore_slotaddress(store_h, store_h->active_bank, store_h->next_slot);
  if (BSP_Flash_Write(address, store_h->slot_buffer, sizeof(store_h->slot_buffer)) != BSP_OK) {
    sys_recordstore_skipfailedslot(store_h);
    return SYS_RECORD_STORE_FLASH_ERROR;
  }

  uint8_t verify_buffer[SYS_RECORD_STORE_SLOT_SIZE];
  if (BSP_Flash_Read(address, verify_buffer, sizeof(verify_buffer)) != BSP_OK ||
      memcmp(verify_buffer, store_h->slot_buffer, sizeof(verify_buffer)) != 0) {
    sys_recordstore_skipfailedslot(store_h);
    return SYS_RECORD_STORE_FLASH_ERROR;
  }

  store_h->next_slot++;
  store_h->next_sequence =
      (store_h->next_sequence == UINT32_MAX) ? 1U : (store_h->next_sequence + 1U);
  return SYS_RECORD_STORE_OK;
}
