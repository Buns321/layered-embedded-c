/**
 * @file sys_record_store.h
 * @brief 基于 bsp_flash 的双页 CRC32 追加持久记录存储。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_RECORD_STORE_H
#define LAYERED_CAR_SYS_RECORD_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define SYS_RECORD_STORE_SLOT_SIZE 128U
#define SYS_RECORD_STORE_HEADER_SIZE 20U
#define SYS_RECORD_STORE_PAYLOAD_CAPACITY                                                          \
  (SYS_RECORD_STORE_SLOT_SIZE - SYS_RECORD_STORE_HEADER_SIZE)
#define SYS_RECORD_STORE_BANK_COUNT 2U

/**
 * @brief 双页记录存储操作状态。
 */
typedef enum sys_record_store_status_e {
  SYS_RECORD_STORE_OK = 0U,
  SYS_RECORD_STORE_EMPTY,
  SYS_RECORD_STORE_INVALID_ARG,
  SYS_RECORD_STORE_INVALID_RECORD,
  SYS_RECORD_STORE_BUFFER_TOO_SMALL,
  SYS_RECORD_STORE_FLASH_ERROR,
} sys_record_store_status_t;

/**
 * @brief 双页记录存储对象（静态缓冲，仅允许单写者串行调用）。
 */
typedef struct sys_record_store_s {
  uint32_t bank_address[SYS_RECORD_STORE_BANK_COUNT];
  uint32_t page_size_bytes;
  uint32_t magic;
  uint32_t next_sequence;
  uint16_t slot_count;
  uint16_t next_slot;
  uint8_t active_bank;
  bool initialized;
  uint8_t slot_buffer[SYS_RECORD_STORE_SLOT_SIZE];
} sys_record_store_t;

/**
 * @brief 初始化双页记录存储并扫描最新有效记录；无有效记录时擦除第 0 页。
 */
sys_record_store_status_t Sys_RecordStore_Init(sys_record_store_t *store_h,
                                               uint32_t first_bank_address,
                                               uint32_t page_size_bytes, uint32_t magic);

/**
 * @brief 读取序号最新的有效记录（payload_capacity_bytes 单位 byte）。
 */
sys_record_store_status_t Sys_RecordStore_LoadLatest(const sys_record_store_t *store_h,
                                                     uint16_t *version, void *payload,
                                                     uint32_t payload_capacity_bytes,
                                                     uint32_t *payload_size_bytes);

/**
 * @brief 追加一条记录并回读校验；当前页写满时自动擦除并切换另一页。
 */
sys_record_store_status_t Sys_RecordStore_Append(sys_record_store_t *store_h, uint16_t version,
                                                 const void *payload, uint32_t payload_size_bytes);

/** @} */

#endif /* LAYERED_CAR_SYS_RECORD_STORE_H */
