/**
 * @file sys_byte_ring.h
 * @brief 单生产者/单消费者（SPSC）无锁静态字节环形队列。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_BYTE_RING_H
#define LAYERED_CAR_SYS_BYTE_RING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 字节环形队列对象；capacity_bytes 须为 >= 2 的 2 的幂，实际可用 capacity_bytes - 1。
 */
typedef struct sys_byte_ring_s {
  uint8_t *buffer;
  uint16_t capacity_bytes;
  volatile uint16_t read_index;
  volatile uint16_t write_index;
  volatile uint32_t dropped_bytes;
  bool initialized;
} sys_byte_ring_t;

/**
 * @brief 初始化 SPSC 字节环并绑定静态缓冲区（仅初始化阶段调用）。
 */
bool Sys_ByteRing_Init(sys_byte_ring_t *ring_h, uint8_t *buffer, uint16_t capacity_bytes);

/**
 * @brief 单生产者压入 1 字节（可在 ISR 调用；队列满时丢弃并递增 dropped_bytes）。
 */
bool Sys_ByteRing_PushFromISR(sys_byte_ring_t *ring_h, uint8_t value);

/**
 * @brief 单消费者弹出 1 字节（任务上下文调用）。
 */
bool Sys_ByteRing_Pop(sys_byte_ring_t *ring_h, uint8_t *value);

/**
 * @brief 查询队列当前是否为空。
 */
bool Sys_ByteRing_IsEmpty(const sys_byte_ring_t *ring_h);

/**
 * @brief 查询因队列满而丢弃的累计字节数，单位 byte。
 */
uint32_t Sys_ByteRing_GetDroppedBytes(const sys_byte_ring_t *ring_h);

/** @} */

#endif /* LAYERED_CAR_SYS_BYTE_RING_H */
