/**
 * @file system_byte_ring_contract_test.c
 * @brief System 单生产者/单消费者字节环契约测试。
 * @ingroup tests
 */

#include "system_byte_ring.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief 验证字节环顺序、容量边界、回绕和丢弃计数。
 * @post FIFO 顺序保持，实际容量为 capacity_bytes - 1。
 * @note 测试同步模拟生产者和消费者，不访问硬件。
 */
static void test_ring_operations(void) {
  uint8_t buffer[4];
  sys_byte_ring_t ring;
  assert(Sys_ByteRing_Init(&ring, buffer, sizeof(buffer)));
  assert(Sys_ByteRing_IsEmpty(&ring));

  assert(Sys_ByteRing_PushFromISR(&ring, 1u));
  assert(Sys_ByteRing_PushFromISR(&ring, 2u));
  assert(Sys_ByteRing_PushFromISR(&ring, 3u));
  assert(!Sys_ByteRing_PushFromISR(&ring, 4u));
  assert(Sys_ByteRing_GetDroppedBytes(&ring) == 1u);

  uint8_t value = 0u;
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 1u);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 2u);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 3u);
  assert(!Sys_ByteRing_Pop(&ring, &value));

  assert(Sys_ByteRing_PushFromISR(&ring, 4u));
  assert(Sys_ByteRing_PushFromISR(&ring, 5u));
  assert(Sys_ByteRing_PushFromISR(&ring, 6u));
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 4u);
  assert(Sys_ByteRing_PushFromISR(&ring, 7u));
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 5u);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 6u);
  assert(Sys_ByteRing_Pop(&ring, &value) && value == 7u);
}

/**
 * @brief 验证非法初始化和无效对象访问。
 * @post 空指针、过小容量和非 2 的幂容量被拒绝。
 * @note 不访问硬件。
 */
static void test_validation(void) {
  uint8_t buffer[8];
  sys_byte_ring_t ring;
  assert(!Sys_ByteRing_Init(NULL, buffer, sizeof(buffer)));
  assert(!Sys_ByteRing_Init(&ring, NULL, sizeof(buffer)));
  assert(!Sys_ByteRing_Init(&ring, buffer, 1u));
  assert(!Sys_ByteRing_Init(&ring, buffer, 3u));
  assert(Sys_ByteRing_IsEmpty(NULL));
  assert(Sys_ByteRing_GetDroppedBytes(NULL) == 0u);
  assert(!Sys_ByteRing_PushFromISR(NULL, 1u));
  assert(!Sys_ByteRing_Pop(NULL, &buffer[0]));

  sys_byte_ring_t uninitialized = {
    .dropped_bytes = 7u,
  };
  assert(Sys_ByteRing_GetDroppedBytes(&uninitialized) == 0u);
}

/**
 * @brief 运行字节环契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_ring_operations();
  test_validation();
  return 0;
}
