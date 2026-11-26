/**
 * @file sys_byte_ring.c
 * @brief SPSC 无锁字节环形队列实现。
 * @ingroup sys_core
 */

#include "sys_byte_ring.h"
#include <stdatomic.h>
#include <stddef.h>
#include <string.h>

static uint16_t sys_bytering_mask(uint16_t capacity_bytes) {
  return (uint16_t)(capacity_bytes - 1U);
}

bool Sys_ByteRing_Init(sys_byte_ring_t *ring_h, uint8_t *buffer, uint16_t capacity_bytes) {
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

  uint16_t write_index = ring_h->write_index;
  uint16_t next_write = (uint16_t)((write_index + 1U) & sys_bytering_mask(ring_h->capacity_bytes));
  if (next_write == ring_h->read_index) {
    ring_h->dropped_bytes++;
    return false;
  }

  ring_h->buffer[write_index] = value;
  atomic_signal_fence(memory_order_release);
  ring_h->write_index = next_write;
  return true;
}

bool Sys_ByteRing_Pop(sys_byte_ring_t *ring_h, uint8_t *value) {
  if (ring_h == NULL || value == NULL || !ring_h->initialized) {
    return false;
  }

  uint16_t read_index = ring_h->read_index;
  if (read_index == ring_h->write_index) {
    return false;
  }

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
