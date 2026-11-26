/**
 * @file sys_byte_ring.c
 * @brief SPSC 无锁字节环形队列实现。
 * @details 属于 Layer 2（Framework/Core），依托单生产者独占 write_index、单消费者独占
 *          read_index 的所有权划分以及 C11 atomic_signal_fence 编译屏障，在单核 MCU
 *          上无需关中断即可实现 ISR 与任务间的无锁字节传递。
 * @ingroup Framework_Core
 */

#include "sys_byte_ring.h"
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 计算 2 的幂容量对应的位掩码（capacity_bytes - 1），用于替代取模运算。
 * @param[in] capacity_bytes 队列总容量（必须为 >= 2 的 2 的幂）。
 * @return 用于索引回绕的位掩码。
 */
static uint16_t sys_bytering_mask(uint16_t capacity_bytes) {
  return (uint16_t)(capacity_bytes - 1U);
}

bool Sys_ByteRing_Init(sys_byte_ring_t *ring_h, uint8_t *buffer, uint16_t capacity_bytes) {
  // 校验 capacity_bytes 为 >= 2 的 2 的幂，确保掩码回绕与留空 1 槽判满策略成立
  if (ring_h == NULL || buffer == NULL || capacity_bytes < 2U ||
      (capacity_bytes & (uint16_t)(capacity_bytes - 1U)) != 0U) {
    return false;
  }

  (void)memset(ring_h, 0, sizeof(*ring_h));
  ring_h->buffer = buffer;
  ring_h->capacity_bytes = capacity_bytes;
  ring_h->initialized = true;
  return true;
}

bool Sys_ByteRing_PushFromISR(sys_byte_ring_t *ring_h, uint8_t value) {
  if (ring_h == NULL || !ring_h->initialized) {
    return false;
  }

  // 仅生产者写 write_index；预留 1 字节空槽区分满与空，避免维护额外计数变量
  uint16_t write_index = ring_h->write_index;
  uint16_t next_write = (uint16_t)((write_index + 1U) & sys_bytering_mask(ring_h->capacity_bytes));
  if (next_write == ring_h->read_index) {
    ring_h->dropped_bytes++;
    return false;
  }

  ring_h->buffer[write_index] = value;
  // 禁止编译器将 write_index 更新重排到数据写入之前，确保消费者看到新游标时字节已落盘
  atomic_signal_fence(memory_order_release);
  ring_h->write_index = next_write;
  return true;
}

bool Sys_ByteRing_Pop(sys_byte_ring_t *ring_h, uint8_t *value) {
  if (ring_h == NULL || value == NULL || !ring_h->initialized) {
    return false;
  }

  // 仅消费者写 read_index；在单核 Cortex-M 上 16 位对齐读写具备原子性
  uint16_t read_index = ring_h->read_index;
  if (read_index == ring_h->write_index) {
    return false;
  }

  // 禁止编译器在观测到 write_index 推进之前提前投机读取缓冲区内容
  atomic_signal_fence(memory_order_acquire);
  *value = ring_h->buffer[read_index];
  ring_h->read_index = (uint16_t)((read_index + 1U) & sys_bytering_mask(ring_h->capacity_bytes));
  return true;
}

bool Sys_ByteRing_IsEmpty(const sys_byte_ring_t *ring_h) {
  return ring_h == NULL || !ring_h->initialized || ring_h->read_index == ring_h->write_index;
}

uint32_t Sys_ByteRing_GetDroppedBytes(const sys_byte_ring_t *ring_h) {
  return (ring_h == NULL || !ring_h->initialized) ? 0U : ring_h->dropped_bytes;
}
