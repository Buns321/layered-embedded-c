/**
 * @file sys_snapshot.h
 * @brief 基于短临界区的跨任务一致性结构体快照读写宏与内联函数。
 * @details 属于 Layer 2（Framework/Core），通过临界区封装定长内存拷贝与编译期尺寸断言，
 *          防止多任务并发读写多字节结构体时产生撕裂（torn read/write）；零动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_SNAPSHOT_H
#define LAYERED_CAR_SYS_SNAPSHOT_H

#include "sys_critical.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 在任务临界区内原子覆盖共享快照缓冲区。
 * @param[out] dst        目标共享快照缓冲区指针（不可为 NULL）。
 * @param[in]  src        源结构体缓冲区指针（不可为 NULL）。
 * @param[in]  size_bytes 拷贝字节数（单位：B）。
 * @pre 仅限任务上下文调用（内部使用 Sys_Critical_Enter/Exit）。
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
 * @brief 在任务临界区内原子读取共享快照副本。
 * @param[out] dst        接收副本的本地结构体指针（不可为 NULL）。
 * @param[in]  src        共享快照缓冲区指针（不可为 NULL）。
 * @param[in]  size_bytes 拷贝字节数（单位：B）。
 * @pre 仅限任务上下文调用（内部使用 Sys_Critical_Enter/Exit）。
 */
static inline void Sys_Snapshot_Read(void *dst, const void *src, size_t size_bytes) {
  assert(dst != NULL && src != NULL);

  Sys_Critical_Enter();
  if (size_bytes > 0U) {
    (void)memcpy(dst, src, size_bytes);
  }
  Sys_Critical_Exit();
}

/**
 * @brief 带编译期尺寸断言的快照写入宏。
 * @param dst_ptr 目标共享结构体指针。
 * @param src_ptr 源本地结构体指针。
 */
#define SYS_SNAPSHOT_WRITE(dst_ptr, src_ptr)                                                       \
  do {                                                                                             \
    _Static_assert(sizeof(*(dst_ptr)) == sizeof(*(src_ptr)), "snapshot write size mismatch");      \
    Sys_Snapshot_Write((dst_ptr), (src_ptr), sizeof(*(dst_ptr)));                                  \
  } while (0)

/**
 * @brief 带编译期尺寸断言的快照读取宏。
 * @param dst_ptr 接收副本的本地结构体指针。
 * @param src_ptr 共享结构体指针。
 */
#define SYS_SNAPSHOT_READ(dst_ptr, src_ptr)                                                        \
  do {                                                                                             \
    _Static_assert(sizeof(*(dst_ptr)) == sizeof(*(src_ptr)), "snapshot read size mismatch");       \
    Sys_Snapshot_Read((dst_ptr), (src_ptr), sizeof(*(dst_ptr)));                                   \
  } while (0)

/** @} */

#endif /* LAYERED_CAR_SYS_SNAPSHOT_H */
