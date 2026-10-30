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
 * @param crc 当前 CRC32 累加状态；标准 CRC32 初值为 0xFFFFFFFF。
 * @param data 非空只读字节缓冲区；size 为 0 时允许不访问。
 * @param size 待处理字节数，单位 byte。
 * @return 更新后的 CRC32 累加状态，尚未取反；调用方负责最终异或。
 * @pre data 在 size > 0 时覆盖至少 size 个可读字节。
 * @post 不修改输入缓冲区或全局状态。
 * @note 私有函数，使用反射多项式 0xEDB88320；不分配动态内存。
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
 * @param header 非空记录头，payload_size 必须已设置且不超过负载容量。
 * @param payload 非空负载缓冲区；header->payload_size 为 0 时允许不访问。
 * @return 头字段（不含 header_crc）与 payload_size 字节负载计算出的最终 CRC32。
 * @pre payload 覆盖 header->payload_size 个可读字节。
 * @post 不修改头、负载或全局状态。
 * @note 私有函数，不把 header_crc 字段计入 CRC；不分配动态内存。
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
 * @param store_h 非空且已初始化的记录存储对象。
 * @param bank 页索引，范围 0..SYSTEM_RECORD_STORE_BANK_COUNT-1。
 * @param slot 页内槽位索引，必须小于 store_h->slot_count。
 * @return 该槽位的绝对 Flash 地址，单位 byte。
 * @pre store_h 已初始化，bank 和 slot 在有效范围内。
 * @post 只计算结果，不访问或修改 Flash。
 * @note 私有函数，不执行边界检查；越界索引会产生错误地址。
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
 * @param header 非空记录头，取自一个完整槽位缓冲区。
 * @retval true magic 和 sequence 均为擦除态 0xFFFFFFFF。
 * @retval false 至少一个字段已被写入，需要进一步校验。
 * @pre header 指向至少包含完整记录头的可读存储。
 * @post 不修改记录头或 Flash。
 * @note 私有函数，不判断 CRC 或其他字段；不分配动态内存。
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
 * @param store_h 非空且已初始化的记录存储对象，提供期望 magic 和负载容量。
 * @param header 非空记录头，必须与 slot 指向缓冲区的前缀一致。
 * @param slot 非空完整槽位缓冲区，包含记录头和负载。
 * @retval true magic 匹配、payload_size 在 1..容量内，且头/负载 CRC 与 header_crc 一致。
 * @retval false 任一字段或 CRC 不满足约束；该槽位应被扫描跳过。
 * @pre store_h 已初始化，header 与 slot 至少覆盖
 *      SYSTEM_RECORD_STORE_HEADER_SIZE + header->payload_size 字节。
 * @post 不修改输入缓冲区或 Flash。
 * @note 私有函数，不检查 sequence；不分配动态内存。
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
 * @param store_h 非空且已初始化的记录存储对象。
 * @param location 非空输出位置；成功或空结果时写入最新槽位信息。
 * @retval SYSTEM_RECORD_STORE_OK 找到最新有效记录，location->found 为 true。
 * @retval SYSTEM_RECORD_STORE_EMPTY 没有非空且 CRC 有效的记录，found 为 false。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR 任一槽位读取失败。
 * @pre store_h 已初始化，location 非 NULL。
 * @post OK/EMPTY 时 location 已完整写入；FLASH_ERROR 时 location 保持不变。
 *       最新记录按 sequence 的有符号差值判定，支持序号回绕。
 * @note 私有函数，每次读取全部 2 * slot_count 个槽位并跳过擦除态/损坏槽；
 *       不分配动态内存。必须由单写者串行调用。
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
 * @param store_h 非空且已初始化的记录存储对象。
 * @param bank 待擦除页索引，范围 0..SYSTEM_RECORD_STORE_BANK_COUNT-1。
 * @retval SYSTEM_RECORD_STORE_OK 整页擦除成功，槽位恢复为 0xFF 空标记。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR Flash 擦除失败，页内容状态不确定。
 * @pre store_h 已初始化，bank 有效，且没有其他代码访问该页。
 * @post 成功时页内旧记录全部失效；失败时可能已发生部分擦除，不能继续按旧内容使用。
 * @note 私有函数，不分配动态内存；Flash 擦除可能阻塞并受掉电影响。
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
 * @param store_h 非空且已初始化的记录存储对象。
 * @pre store_h->next_slot <= store_h->slot_count，且当前槽位写入或回读失败。
 * @post next_slot 未到页尾时前进一格，已到页尾时保持不变；next_sequence 不变。
 * @note 私有函数，避免重试可能已部分编程的槽位；不分配动态内存。
 */
static void sys_recordstore_skipfailedslot(system_record_store_t *store_h) {
  assert(store_h);

  if (store_h->next_slot < store_h->slot_count) {
    store_h->next_slot++;
  }
}

/**
 * @brief 初始化双页记录存储并扫描最新有效记录。
 * @param store_h 待初始化对象；调用方拥有，生命周期必须覆盖后续全部存储操作。
 * @param flash 已初始化的内部 Flash 实例，按值复制到 store_h。
 * @param first_bank_address 第一页起始地址，必须按 page_size 对齐。
 * @param page_size 单页大小，单位 byte；必须等于两个页的 Flash 擦除单元，且为
 *                  SYSTEM_RECORD_STORE_SLOT_SIZE 的整数倍。
 * @param magic 应用记录 Magic，不能为 0xFFFFFFFF。
 * @retval SYSTEM_RECORD_STORE_OK 扫描和初始化完成，槽位位置及下一序号已就绪。
 * @retval SYSTEM_RECORD_STORE_INVALID_ARGUMENT store_h 为 NULL、页大小/地址不满足约束，
 *         或 magic 等于空标记。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR 扫描读取或首次擦除失败，对象未标记为可写。
 * @pre flash 已初始化；两个页的地址范围落在该 Flash 内，且由本存储独占。
 * @post 成功时 initialized=true 并按最新有效记录设置 active_bank、next_slot、
 *       next_sequence；无有效记录时擦除 bank 0 并从序号 1 开始。失败时不标记为可写。
 * @note 不分配动态内存；初始化会扫描全部槽位。只允许单写者，应在任务启动前调用。
 * @warning 擦除会破坏目标页旧数据；页地址不得与代码或其他持久数据重叠。
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
 * @param store_h 已成功初始化的记录存储对象。
 * @param version 非空输出指针；成功时写入记录版本。
 * @param payload 调用方拥有的输出缓冲区；容量由 payload_capacity 指定。
 * @param payload_capacity 输出缓冲区容量，单位 byte。
 * @param payload_size 非空输出指针；成功时写入实际负载长度，单位 byte。
 * @retval SYSTEM_RECORD_STORE_OK 找到最新有效记录并完整复制。
 * @retval SYSTEM_RECORD_STORE_EMPTY 两页中都没有有效记录。
 * @retval SYSTEM_RECORD_STORE_INVALID_ARGUMENT store_h 未初始化，或任一输出指针为 NULL。
 * @retval SYSTEM_RECORD_STORE_INVALID_RECORD 扫描后再次读取时记录头或 CRC 失效。
 * @retval SYSTEM_RECORD_STORE_BUFFER_TOO_SMALL payload_capacity 小于记录负载长度。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR 扫描或最终读取失败。
 * @pre store_h 已初始化，version、payload、payload_size 非 NULL。
 * @post 仅成功时写入 version、payload 和 payload_size；失败时不保证输出内容，
 *       且不修改 store_h。负载不追加 NUL 终止符。
 * @note 每次调用都会重新扫描 Flash 槽位，复杂度为 O(2 * slot_count)。不分配动态内存；
 *       必须与 Sys_RecordStore_Append 串行使用，避免读取到写入中的槽位。
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
 * @param store_h 已成功初始化且由单一写者拥有的记录存储对象。
 * @param version 应用记录版本，原样写入记录头。
 * @param payload 非空只读负载缓冲区，只在本次调用期间访问。
 * @param payload_size 负载长度，单位 byte，范围 1..SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY。
 * @retval SYSTEM_RECORD_STORE_OK 写入、回读比较和序号推进均成功。
 * @retval SYSTEM_RECORD_STORE_INVALID_ARGUMENT store_h 未初始化，或 payload/payload_size 非法。
 * @retval SYSTEM_RECORD_STORE_FLASH_ERROR 目标页擦除、槽位写入或回读校验失败。
 * @pre store_h 已初始化；payload 容量至少为 payload_size；同一对象没有并发写者。
 * @post 成功时 next_slot 前进一格，next_sequence 前进一次（UINT32_MAX 后回到 1）。
 *       写入或回读失败时跳过故障槽且不推进序号；若故障槽已是页尾，next_slot 保持页尾。
 *       页满时先擦除并切换另一页。
 * @note 只允许单一写者从任务上下文调用；payload 不必在函数返回后继续有效。
 *       不分配动态内存，Flash 操作可能阻塞。
 * @warning 擦除/写入期间掉电可能使新槽位无效；扫描会忽略坏槽并从上一有效记录恢复。
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
