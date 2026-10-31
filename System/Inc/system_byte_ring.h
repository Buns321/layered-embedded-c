/**
 * @file system_byte_ring.h
 * @brief 单生产者/单消费者字节环形队列。
 * @addtogroup system
 * @{
 * @note 缓冲区由调用方静态提供；队列保留一个空槽区分满和空。
 * @warning 只支持一个生产者和一个消费者操作同一实例；多核共享时调用方还需提供内存序保证。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_BYTE_RING_H
#define LAYERED_EMBEDDED_C_SYSTEM_BYTE_RING_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 字节环形队列对象。
 */
typedef struct sys_byte_ring_s {
  /** @brief 调用方提供的缓冲存储，不得在对象使用期间释放。 */
  uint8_t *buffer;
  /** @brief 缓冲容量，单位 byte；必须为 2 的幂。 */
  uint16_t capacity_bytes;
  /** @brief 下一个可读下标，只由消费者修改。 */
  volatile uint16_t read_index;
  /** @brief 下一个可写下标，只由生产者修改。 */
  volatile uint16_t write_index;
  /** @brief 因队列满而丢弃的字节总数，由生产者修改。 */
  volatile uint32_t dropped_bytes;
  /** @brief 初始化完成标志。 */
  bool initialized;
} sys_byte_ring_t;

/**
 * @brief 初始化单生产者/单消费者字节环。
 * @param ring_h 待初始化队列，不得为 NULL。
 * @param buffer 调用方提供的静态缓冲，不得为 NULL。
 * @param capacity_bytes 缓冲容量，单位 byte；必须为不小于 2 的 2 的幂。
 * @retval true 队列已清零并绑定缓冲区。
 * @retval false 指针为空或容量不满足要求。
 * @pre ring_h 和 buffer 在队列生命周期内保持有效。
 * @post 成功时读写下标和丢弃计数均为 0，队列为空。
 * @note 不分配内存；只允许初始化阶段单线程调用。
 */
bool Sys_ByteRing_Init(
  sys_byte_ring_t *ring_h,
  uint8_t *buffer,
  uint16_t capacity_bytes
);

/**
 * @brief 由单一生产者写入一个字节。
 * @param ring_h 已初始化队列，不得为 NULL。
 * @param value 待写入字节。
 * @retval true 字节已写入。
 * @retval false 队列已满或对象无效；满时递增 dropped_bytes。
 * @pre 只允许一个生产者调用，可在任务或 ISR 上下文执行。
 * @post 成功时写指针推进一格；失败时除非队列满，否则对象不变。
 * @note 不阻塞、不分配内存；多核场景不提供跨核同步。
 */
bool Sys_ByteRing_PushFromISR(sys_byte_ring_t *ring_h, uint8_t value);

/**
 * @brief 由单一消费者读取一个字节。
 * @param ring_h 已初始化队列，不得为 NULL。
 * @param value 输出字节存储，不得为 NULL。
 * @retval true value 已写入，读指针推进一格。
 * @retval false 队列为空或对象无效，value 不变。
 * @pre 只允许一个消费者调用，通常位于任务上下文。
 * @post 成功时该字节从队列逻辑移除，缓冲内容不复位。
 * @note 不阻塞、不分配内存。
 */
bool Sys_ByteRing_Pop(sys_byte_ring_t *ring_h, uint8_t *value);

/**
 * @brief 判断字节环是否为空。
 * @param ring_h 队列对象，可为 NULL。
 * @return 对象无效或读写下标相等时返回 true，否则返回 false。
 * @pre 无。
 * @post 不修改队列状态。
 * @note 结果是瞬时快照；并发生产者可能立即改变可见状态。
 */
bool Sys_ByteRing_IsEmpty(const sys_byte_ring_t *ring_h);

/**
 * @brief 读取因环形缓冲满而丢弃的字节总数。
 * @param ring_h 队列对象，可为 NULL。
 * @return 初始化以来的丢弃字节数；对象无效时返回 0。
 * @pre 无。
 * @post 不修改队列状态。
 * @note 32 位计数溢出后自然回绕；不分配内存。
 */
uint32_t Sys_ByteRing_GetDroppedBytes(const sys_byte_ring_t *ring_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_BYTE_RING_H
