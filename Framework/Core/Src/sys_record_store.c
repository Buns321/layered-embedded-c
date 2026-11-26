/**
 * @file sys_record_store.c
 * @brief 基于 bsp_flash 的双页 CRC32 记录存储实现。
 * @details 属于 Layer 2（Framework/Core），采用“追加写 + 先写新页再擦除/复用旧页”的
 *          乒乓机制，每条记录均携带递增序号与覆盖头部及载荷的 CRC32 校验，确保写入过程
 *          遭遇掉电时仍可回退至上一条有效记录。
 * @ingroup Framework_Core
 */

#include "sys_record_store.h"
#include "bsp_flash.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/** @brief Flash 擦除后的空白标记值（全 1）。 */
#define SYS_RECORD_STORE_EMPTY_MARKER 0xFFFFFFFFU

/** @brief 标准 IEEE 802.3 反射 CRC32 多项式（0xEDB88320）。 */
#define SYS_RECORD_STORE_CRC_POLYNOMIAL 0xEDB88320U

/**
 * @brief Flash 槽位固定头部布局（共 20 字节，与 SYS_RECORD_STORE_HEADER_SIZE 严格一致）。
 */
typedef struct sys_record_store_header_s {
  uint32_t magic;        /**< 业务魔数标识（0xFFFFFFFF 表示未写入空白槽位）。 */
  uint16_t version;      /**< 业务数据结构版本号。 */
  uint16_t payload_size; /**< 有效载荷字节长度（单位：B）。 */
  uint32_t sequence;     /**< 单调递增记录序号（1..UINT32_MAX 回绕）。 */
  uint32_t payload_crc;  /**< 保留字段（当前统一由 header_crc 覆盖头部与载荷）。 */
  uint32_t header_crc;   /**< 覆盖 header[0..header_crc) 与 payload 的 CRC32 校验值。 */
} sys_record_store_header_t;

/**
 * @brief 扫描得到的最新有效记录物理位置描述。
 */
typedef struct sys_record_store_location_s {
  uint32_t sequence; /**< 最新有效记录的递增序号。 */
  uint16_t slot;     /**< 所在页内的槽位索引。 */
  uint8_t bank;      /**< 所在 Flash 页索引（0 或 1）。 */
  bool found;        /**< 是否在双页中找到了至少一条有效记录。 */
} sys_record_store_location_t;

_Static_assert(sizeof(sys_record_store_header_t) == SYS_RECORD_STORE_HEADER_SIZE,
               "record store header layout changed");
_Static_assert(SYS_RECORD_STORE_SLOT_SIZE % 4U == 0U, "record store slot must be word aligned");
_Static_assert(SYS_RECORD_STORE_HEADER_SIZE % 4U == 0U, "record store header must be word aligned");

/**
 * @brief 增量更新 IEEE 802.3 CRC32 校验值（无查找表实现以节省 Flash 常量区）。
 * @param[in] crc  当前 CRC32 中间值。
 * @param[in] data 待计算字节流首地址。
 * @param[in] size 待计算字节数（单位：B）。
 * @return 更新后的 CRC32 中间值。
 */
static uint32_t sys_recordstore_crc32update(uint32_t crc, const void *data, uint32_t size) {
  const uint8_t *bytes = (const uint8_t *)data;
  for (uint32_t index = 0U; index < size; index++) {
    crc ^= bytes[index];
    for (uint8_t bit = 0U; bit < 8U; bit++) {
      // 无分支掩码：最低位为 1 时 mask=0xFFFFFFFF，否则为 0
      uint32_t mask = 0U - (crc & 1U);
      crc = (crc >> 1U) ^ (SYS_RECORD_STORE_CRC_POLYNOMIAL & mask);
    }
  }
  return crc;
}

/**
 * @brief 计算单条记录（头部前缀 + 载荷）的最终 CRC32 校验值。
 * @param[in] header  指向记录头部的只读指针。
 * @param[in] payload 指向载荷字节区的只读指针。
 * @return 取反后的最终 CRC32 值。
 */
static uint32_t sys_recordstore_recordcrc(const sys_record_store_header_t *header,
                                          const uint8_t *payload) {
  uint32_t crc = 0xFFFFFFFFU;
  crc = sys_recordstore_crc32update(crc, header, offsetof(sys_record_store_header_t, header_crc));
  crc = sys_recordstore_crc32update(crc, payload, header->payload_size);
  return ~crc;
}

/**
 * @brief 计算指定页与槽位在 Flash 中的物理字节地址。
 * @param[in] store_h 记录存储对象指针。
 * @param[in] bank    页索引（0 或 1）。
 * @param[in] slot    槽位索引。
 * @return 物理地址（单位：B）。
 */
static uint32_t sys_recordstore_slotaddress(const sys_record_store_t *store_h, uint8_t bank,
                                            uint16_t slot) {
  return store_h->bank_address[bank] + ((uint32_t)slot * SYS_RECORD_STORE_SLOT_SIZE);
}

/**
 * @brief 判断槽位头部是否处于擦除后的全 0xFF 空白状态。
 * @param[in] header 槽位头部指针。
 * @return 若为空白槽位返回 true。
 */
static bool sys_recordstore_headerisempty(const sys_record_store_header_t *header) {
  return header->magic == SYS_RECORD_STORE_EMPTY_MARKER &&
         header->sequence == SYS_RECORD_STORE_EMPTY_MARKER;
}

/**
 * @brief 校验槽位头部的魔数、载荷长度边界及 CRC32 完整性。
 * @param[in] store_h 记录存储对象指针。
 * @param[in] header  待校验头部指针。
 * @param[in] slot    完整槽位原始字节缓冲区（长度为 SYS_RECORD_STORE_SLOT_SIZE）。
 * @return 槽位数据是否完整有效。
 */
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

/**
 * @brief 全量扫描双页所有槽位，定位序号最新的有效记录。
 * @param[in]  store_h  记录存储对象指针。
 * @param[out] location 输出最新有效记录的位置描述。
 * @return 扫描状态（OK / EMPTY / FLASH_ERROR）。
 */
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

      // 采用有符号差值比较处理 32 位序号回绕（假定两页有效记录序号跨度远小于 2^31）
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

/**
 * @brief 擦除指定索引的 Flash 存储页。
 * @param[in,out] store_h 记录存储对象指针。
 * @param[in]     bank    目标页索引（0 或 1）。
 * @return 擦除状态（OK 或 FLASH_ERROR）。
 */
static sys_record_store_status_t sys_recordstore_erasebank(sys_record_store_t *store_h,
                                                           uint8_t bank) {
  if (BSP_Flash_Erase(store_h->bank_address[bank], store_h->page_size_bytes) != BSP_OK) {
    return SYS_RECORD_STORE_FLASH_ERROR;
  }
  return SYS_RECORD_STORE_OK;
}

/**
 * @brief 在写入或回读校验失败后跳过当前受损槽位，防止下次重复覆盖同一物理单元。
 * @param[in,out] store_h 记录存储对象指针。
 */
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

  // 防地址算术溢出校验：确保双页连续地址空间不跨越 UINT32_MAX
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
    // 双页均无有效记录（首次上电或历史数据全损坏）：主动擦除第 0 页确保干净起点
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

  // 当前页写满时先擦除备用页并切换过去，确保擦除期间当前页上的历史最新记录始终完好
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

  // 写入后立即回读整槽位做逐字节比对，拦截 Flash 编程静默位翻转
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
