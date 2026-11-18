/**
 * @file system_snapshot.h
 * @brief 基于短临界区的跨任务状态快照读写原语。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_SNAPSHOT_H
#define LAYERED_EMBEDDED_C_SYSTEM_SNAPSHOT_H

#include "system_critical.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 在任务上下文临界区内写入整块快照数据。
 * @param dst 目标共享快照缓冲区，不得为 NULL，生命周期须覆盖调用。
 * @param src 源本地数据缓冲区，不得为 NULL，不得与 dst 重叠。
 * @param size_bytes 拷贝字节数，单位 byte；为 0 时仅执行空临界区。
 * @pre 只允许在任务上下文或调度器启动前调用；不得在 ISR 中使用。
 * @post dst 中的 size_bytes 字节已被 src 原子性覆盖。
 * @note 不分配动态内存；仅用于短小结构体复制，避免阻塞或复杂运算。
 */
static inline void Sys_Snapshot_Write(void *dst, const void *src, size_t size_bytes) {
  assert(dst != NULL && src != NULL);

  Sys_Critical_Enter();
  if (size_bytes > 0u) {
    (void)memcpy(dst, src, size_bytes);
  }
  Sys_Critical_Exit();
}

/**
 * @brief 在任务上下文临界区内读取整块快照数据。
 * @param dst 调用方持有的本地输出缓冲区，不得为 NULL。
 * @param src 源共享快照缓冲区，不得为 NULL，不得与 dst 重叠。
 * @param size_bytes 拷贝字节数，单位 byte；为 0 时仅执行空临界区。
 * @pre 只允许在任务上下文或调度器启动前调用；不得在 ISR 中使用。
 * @post dst 中保存 src 在同一临界区时刻的一致副本。
 * @note 不分配动态内存；仅用于短小结构体复制。
 */
static inline void Sys_Snapshot_Read(void *dst, const void *src, size_t size_bytes) {
  assert(dst != NULL && src != NULL);

  Sys_Critical_Enter();
  if (size_bytes > 0u) {
    (void)memcpy(dst, src, size_bytes);
  }
  Sys_Critical_Exit();
}

/**
 * @brief 带编译期尺寸校验的临界区快照写入宏。
 * @param dst_ptr 目标共享对象指针，不得为 NULL。
 * @param src_ptr 源本地对象指针，不得为 NULL。
 * @note 通过 _Static_assert 校验两端对象尺寸一致；参数各自仅求值一次。
 */
#define SYS_SNAPSHOT_WRITE(dst_ptr, src_ptr)                                                       \
  do {                                                                                             \
    _Static_assert(sizeof(*(dst_ptr)) == sizeof(*(src_ptr)), "snapshot write size mismatch");      \
    Sys_Snapshot_Write((dst_ptr), (src_ptr), sizeof(*(dst_ptr)));                                  \
  } while (0)

/**
 * @brief 带编译期尺寸校验的临界区快照读取宏。
 * @param dst_ptr 目标本地对象指针，不得为 NULL。
 * @param src_ptr 源共享对象指针，不得为 NULL。
 * @note 通过 _Static_assert 校验两端对象尺寸一致；参数各自仅求值一次。
 */
#define SYS_SNAPSHOT_READ(dst_ptr, src_ptr)                                                        \
  do {                                                                                             \
    _Static_assert(sizeof(*(dst_ptr)) == sizeof(*(src_ptr)), "snapshot read size mismatch");       \
    Sys_Snapshot_Read((dst_ptr), (src_ptr), sizeof(*(dst_ptr)));                                   \
  } while (0)

/** @} */

#endif // LAYERED_EMBEDDED_C_SYSTEM_SNAPSHOT_H
