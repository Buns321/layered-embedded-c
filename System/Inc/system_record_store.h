//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file system_record_store.h
 * @brief 基于内部 Flash 双页追加记录的静态存储。
 * @addtogroup system
 * @{
 * @note 每条记录包含 Magic、版本、负载长度、序号和 CRC32。新记录追加到当前
 *       数据页；写满后擦除另一页再切换，保留上一页作为掉电恢复边界。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_RECORD_STORE_H
#define LAYERED_EMBEDDED_C_SYSTEM_RECORD_STORE_H

#include "platform_flash.h"
#include <stdbool.h>
#include <stdint.h>

#define SYSTEM_RECORD_STORE_SLOT_SIZE 128u
#define SYSTEM_RECORD_STORE_HEADER_SIZE 20u
#define SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY \
  (SYSTEM_RECORD_STORE_SLOT_SIZE - SYSTEM_RECORD_STORE_HEADER_SIZE)
#define SYSTEM_RECORD_STORE_BANK_COUNT 2u

/**
 * @brief 记录存储操作状态。
 */
typedef enum system_record_store_status_e {
  SYSTEM_RECORD_STORE_OK = 0U,
  SYSTEM_RECORD_STORE_EMPTY,
  SYSTEM_RECORD_STORE_INVALID_ARGUMENT,
  SYSTEM_RECORD_STORE_INVALID_RECORD,
  SYSTEM_RECORD_STORE_BUFFER_TOO_SMALL,
  SYSTEM_RECORD_STORE_FLASH_ERROR,
} system_record_store_status_t;

/**
 * @brief 记录存储对象。
 * @note 所有缓冲静态分配；对象生命周期必须覆盖调用者。
 */
typedef struct system_record_store_s {
  /** @brief 底层 Flash 实例。 */
  platform_flash_instance_t flash;
  /** @brief 两个记录页的起始地址。 */
  uint32_t bank_address[SYSTEM_RECORD_STORE_BANK_COUNT];
  /** @brief 单页大小，单位 byte。 */
  uint32_t page_size;
  /** @brief 应用记录 Magic。 */
  uint32_t magic;
  /** @brief 下一条记录序号。 */
  uint32_t next_sequence;
  /** @brief 每页槽位数。 */
  uint16_t slot_count;
  /** @brief 当前页下一个待写槽位。 */
  uint16_t next_slot;
  /** @brief 当前写入页索引。 */
  uint8_t active_bank;
  /** @brief 初始化和扫描是否成功完成。 */
  bool initialized;
  /** @brief 追加记录时使用的静态槽位缓冲。 */
  uint8_t slot_buffer[SYSTEM_RECORD_STORE_SLOT_SIZE];
} system_record_store_t;

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
);

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
);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_RECORD_STORE_H
