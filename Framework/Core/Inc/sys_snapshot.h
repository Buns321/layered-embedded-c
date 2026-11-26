/**
 * @file sys_snapshot.h
 * @brief 基于短临界区的跨任务一致性结构体快照读写宏。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_SNAPSHOT_H
#define LAYERED_CAR_SYS_SNAPSHOT_H

#include "sys_critical.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 在任务临界区内原子覆盖共享快照缓冲区（仅任务上下文调用）。
 */
static inline void Sys_Snapshot_Write(void *dst, const void *src, size_t size_bytes) {
  assert(dst != NULL && src != NULL);

  Sys_Critical_Enter();
  if (size_bytes > 0U) {
    (void)memcpy(dst, src, size_bytes);
  }
  Sys_Critical_Exit();
}

/**
 * @brief 在任务临界区内原子读取共享快照副本（仅任务上下文调用）。
 */
static inline void Sys_Snapshot_Read(void *dst, const void *src, size_t size_bytes) {
  assert(dst != NULL && src != NULL);

  Sys_Critical_Enter();
  if (size_bytes > 0U) {
    (void)memcpy(dst, src, size_bytes);
  }
  Sys_Critical_Exit();
}

/** @brief 带编译期尺寸断言的快照写入宏。 */
#define SYS_SNAPSHOT_WRITE(dst_ptr, src_ptr)                                                       \
  do {                                                                                             \
    _Static_assert(sizeof(*(dst_ptr)) == sizeof(*(src_ptr)), "snapshot write size mismatch");      \
    Sys_Snapshot_Write((dst_ptr), (src_ptr), sizeof(*(dst_ptr)));                                  \
  } while (0)

/** @brief 带编译期尺寸断言的快照读取宏。 */
#define SYS_SNAPSHOT_READ(dst_ptr, src_ptr)                                                        \
  do {                                                                                             \
    _Static_assert(sizeof(*(dst_ptr)) == sizeof(*(src_ptr)), "snapshot read size mismatch");       \
    Sys_Snapshot_Read((dst_ptr), (src_ptr), sizeof(*(dst_ptr)));                                   \
  } while (0)

/** @} */

#endif /* LAYERED_CAR_SYS_SNAPSHOT_H */
