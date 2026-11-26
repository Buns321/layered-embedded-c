/**
 * @file sys_byte_ring.h
 * @brief 单生产者/单消费者（SPSC）无锁静态字节环形队列。
 * @details 属于 Layer 2（Framework/Core），供 BSP 串口接收 ISR 与上层协议解析任务之间
 *          零锁传递字节流；缓冲区由调用方静态提供，零动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_BYTE_RING_H
#define LAYERED_CAR_SYS_BYTE_RING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief SPSC 字节环形队列对象（capacity_bytes 须为 >= 2 的 2 的幂，实际可用 capacity_bytes - 1）。
 */
typedef struct sys_byte_ring_s {
  uint8_t *buffer;                 /**< 外部静态分配的字节存储区首地址。 */
  uint16_t capacity_bytes;         /**< 缓冲区总容量（单位：B，须为 >= 2 的 2 的幂）。 */
  volatile uint16_t read_index;    /**< 消费者独占更新的读游标（0..capacity_bytes-1）。 */
  volatile uint16_t write_index;   /**< 生产者独占更新的写游标（0..capacity_bytes-1）。 */
  volatile uint32_t dropped_bytes; /**< 因队列满而被丢弃的累计字节数（单位：B）。 */
  bool initialized;                /**< 队列是否已完成 Sys_ByteRing_Init() 初始化。 */
} sys_byte_ring_t;

/**
 * @brief 初始化 SPSC 字节环并绑定静态缓冲区。
 * @param[out] ring_h         字节环形队列对象指针（不可为 NULL）。
 * @param[in]  buffer         调用方静态分配的字节缓冲区（不可为 NULL）。
 * @param[in]  capacity_bytes 缓冲区字节长度（单位：B，须为 >= 2 的 2 的幂）。
 * @return 初始化是否成功。
 * @retval true  参数合法且队列已重置就绪。
 * @retval false 指针为空或 capacity_bytes 非 >= 2 的 2 的幂。
 * @pre 仅在初始化阶段调用，且必须在启用相关 ISR 之前完成。
 */
bool Sys_ByteRing_Init(sys_byte_ring_t *ring_h, uint8_t *buffer, uint16_t capacity_bytes);

/**
 * @brief 单生产者压入 1 字节（队列满时丢弃并递增 dropped_bytes）。
 * @param[in,out] ring_h 字节环形队列对象指针（允许为 NULL，为 NULL 时返回 false）。
 * @param[in]     value  待写入的 1 字节数据。
 * @return 写入是否成功。
 * @retval true  字节已写入缓冲区并推进 write_index。
 * @retval false 队列未初始化或已满（已累加 dropped_bytes）。
 * @note 仅允许单一生产者（典型为单个 UART RX ISR）调用，严禁多生产者并发压入。
 */
bool Sys_ByteRing_PushFromISR(sys_byte_ring_t *ring_h, uint8_t value);

/**
 * @brief 单消费者弹出 1 字节。
 * @param[in,out] ring_h 字节环形队列对象指针（允许为 NULL）。
 * @param[out]    value  接收弹出字节的输出指针（不可为 NULL）。
 * @return 读取是否成功。
 * @retval true  成功弹出 1 字节至 *value。
 * @retval false 队列为空、参数无效或未初始化。
 * @note 仅允许单一消费者任务调用，严禁多消费者并发弹出。
 */
bool Sys_ByteRing_Pop(sys_byte_ring_t *ring_h, uint8_t *value);

/**
 * @brief 查询队列当前是否为空。
 * @param[in] ring_h 字节环形队列只读指针（允许为 NULL）。
 * @return 队列是否为空（NULL 或未初始化均返回 true）。
 */
bool Sys_ByteRing_IsEmpty(const sys_byte_ring_t *ring_h);

/**
 * @brief 查询因队列满而丢弃的累计字节数。
 * @param[in] ring_h 字节环形队列只读指针（允许为 NULL）。
 * @return 累计丢弃字节数（单位：B；NULL 或未初始化返回 0）。
 */
uint32_t Sys_ByteRing_GetDroppedBytes(const sys_byte_ring_t *ring_h);

/** @} */

#endif /* LAYERED_CAR_SYS_BYTE_RING_H */
