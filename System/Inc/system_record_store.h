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
);

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
);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_RECORD_STORE_H
