/**
 * @file sys_record_store.h
 * @brief 基于 bsp_flash 的双页 CRC32 追加持久记录存储。
 * @details 属于 Layer 2（Framework/Core），通过双 Flash 页乒乓追加写与 CRC32 完整性校验
 *          实现掉电安全的参数持久化；所有缓冲区均内嵌于静态对象，零动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_RECORD_STORE_H
#define LAYERED_CAR_SYS_RECORD_STORE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 单个 Flash 记录槽位固定字节大小（单位：B，须为 4 字节对齐）。 */
#define SYS_RECORD_STORE_SLOT_SIZE 128U

/** @brief 记录槽位头部固定字节大小（单位：B，对应 sys_record_store_header_t）。 */
#define SYS_RECORD_STORE_HEADER_SIZE 20U

/** @brief 单条记录允许承载的最大载荷字节数（单位：B）。 */
#define SYS_RECORD_STORE_PAYLOAD_CAPACITY                                                          \
  (SYS_RECORD_STORE_SLOT_SIZE - SYS_RECORD_STORE_HEADER_SIZE)

/** @brief 乒乓双页存储区数量（固定为 2 页）。 */
#define SYS_RECORD_STORE_BANK_COUNT 2U

/**
 * @brief 双页记录存储操作状态枚举。
 */
typedef enum sys_record_store_status_e {
  SYS_RECORD_STORE_OK = 0U,          /**< 操作成功完成。 */
  SYS_RECORD_STORE_EMPTY,            /**< 双页中尚未发现任何有效记录。 */
  SYS_RECORD_STORE_INVALID_ARG,      /**< 输入参数非法、地址/页尺寸未对齐或对象未初始化。 */
  SYS_RECORD_STORE_INVALID_RECORD,   /**< 读取到的记录魔数或 CRC32 校验失败。 */
  SYS_RECORD_STORE_BUFFER_TOO_SMALL, /**< 调用方提供的输出缓冲区小于记录实际载荷长度。 */
  SYS_RECORD_STORE_FLASH_ERROR,      /**< 底层 Flash 擦除、写入或回读校验失败。 */
} sys_record_store_status_t;

/**
 * @brief 双页记录存储对象（静态缓冲，仅允许单写者串行调用）。
 */
typedef struct sys_record_store_s {
  uint32_t
      bank_address[SYS_RECORD_STORE_BANK_COUNT];   /**< 两页 Flash 区的起始物理地址（单位：B）。 */
  uint32_t page_size_bytes;                        /**< 单页 Flash 擦除单元字节大小（单位：B）。 */
  uint32_t magic;                                  /**< 记录魔数标识，用于区分不同业务格式。 */
  uint32_t next_sequence;                          /**< 下一次追加记录将使用的递增序号。 */
  uint16_t slot_count;                             /**< 单页内可容纳的固定槽位总数（单位：个）。 */
  uint16_t next_slot;                              /**< 当前活跃页内下一个待写入槽位索引。 */
  uint8_t active_bank;                             /**< 当前活跃写入页索引（0 或 1）。 */
  bool initialized;                                /**< 存储对象是否已成功完成 Init 初始化。 */
  uint8_t slot_buffer[SYS_RECORD_STORE_SLOT_SIZE]; /**< 静态槽位拼装缓冲区，避免栈上大数组。 */
} sys_record_store_t;

/**
 * @brief 初始化双页记录存储并扫描最新有效记录；无有效记录时擦除第 0 页。
 * @param[out] store_h            静态分配的存储对象指针（不可为 NULL）。
 * @param[in]  first_bank_address 第 0 页 Flash 起始地址（须按 page_size_bytes 对齐）。
 * @param[in]  page_size_bytes    单页字节大小（单位：B，须与 BSP_Flash_GetEraseUnitSize 一致）。
 * @param[in]  magic              业务魔数标识（不可等于 0xFFFFFFFFU）。
 * @return 操作状态码。
 * @retval SYS_RECORD_STORE_OK          初始化并完成历史记录恢复（或完成空白初始化）。
 * @retval SYS_RECORD_STORE_INVALID_ARG 地址/页大小不对齐、魔数非法或参数为空。
 * @retval SYS_RECORD_STORE_FLASH_ERROR 扫描或擦除第 0 页时发生底层 Flash 错误。
 */
sys_record_store_status_t Sys_RecordStore_Init(sys_record_store_t *store_h,
                                               uint32_t first_bank_address,
                                               uint32_t page_size_bytes, uint32_t magic);

/**
 * @brief 读取序号最新的有效记录。
 * @param[in]  store_h                已初始化的存储对象指针（不可为 NULL）。
 * @param[out] version                输出记录版本号（不可为 NULL）。
 * @param[out] payload                接收载荷数据的缓冲区指针（不可为 NULL）。
 * @param[in]  payload_capacity_bytes 接收缓冲区最大容量（单位：B）。
 * @param[out] payload_size_bytes     输出实际读取的载荷字节数（单位：B，不可为 NULL）。
 * @return 操作状态码。
 * @retval SYS_RECORD_STORE_OK               成功读取最新有效记录。
 * @retval SYS_RECORD_STORE_EMPTY            双页中尚无有效记录。
 * @retval SYS_RECORD_STORE_BUFFER_TOO_SMALL 接收缓冲区容量不足。
 * @retval SYS_RECORD_STORE_INVALID_RECORD   最新槽位回读校验失败。
 * @retval SYS_RECORD_STORE_FLASH_ERROR      底层 Flash 读取失败。
 */
sys_record_store_status_t Sys_RecordStore_LoadLatest(const sys_record_store_t *store_h,
                                                     uint16_t *version, void *payload,
                                                     uint32_t payload_capacity_bytes,
                                                     uint32_t *payload_size_bytes);

/**
 * @brief 追加一条记录并回读校验；当前页写满时自动擦除并切换另一页。
 * @param[in,out] store_h            已初始化的存储对象指针（不可为 NULL）。
 * @param[in]     version            业务载荷版本号。
 * @param[in]     payload            待写入载荷数据指针（不可为 NULL）。
 * @param[in]     payload_size_bytes
 * 载荷字节长度（单位：B，须在 1..SYS_RECORD_STORE_PAYLOAD_CAPACITY）。
 * @return 操作状态码。
 * @retval SYS_RECORD_STORE_OK          写入且回读校验通过。
 * @retval SYS_RECORD_STORE_INVALID_ARG 参数为空、未初始化或载荷长度越界。
 * @retval SYS_RECORD_STORE_FLASH_ERROR 擦除、写入或回读校验失败（已跳过损坏槽位）。
 * @warning 仅允许单写者在任务上下文串行调用，严禁在 ISR 中调用。
 */
sys_record_store_status_t Sys_RecordStore_Append(sys_record_store_t *store_h, uint16_t version,
                                                 const void *payload, uint32_t payload_size_bytes);

/** @} */

#endif /* LAYERED_CAR_SYS_RECORD_STORE_H */
