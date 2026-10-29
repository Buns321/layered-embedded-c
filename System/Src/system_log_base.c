//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log_base.c
 * @brief System 日志后端抽象实现。
 * @ingroup system
 */

#include "system_log_base.h"
#include <assert.h>
#include <stddef.h>

/** @brief 初始化日志后端并绑定静态操作表。 */
void Sys_Log_Base_Init(
  sys_log_base_t *log_h,
  const char *name,
  const sys_log_ops_t *ops
) {
  assert(
    log_h        &&
    name        &&
    ops         &&
    ops->write
  );

  log_h->name = name;
  log_h->ops  = ops;
}

/**
 * @brief 向日志后端写入字节。
 * @param log_h 后端对象。
 * @param data 字节缓冲区。
 * @param size 字节数。
 * @return 已接受的字节数；后端不可用或数据被丢弃时返回 0。
 */
uint32_t Sys_Log_Base_Write(
  sys_log_base_t *log_h,
  const uint8_t *data,
  uint32_t size
) {
  assert(
    log_h         &&
    log_h->ops    &&
    log_h->ops->write &&
    data         &&
    size != 0u
  );

  if (
    log_h == NULL       ||
    log_h->ops == NULL  ||
    log_h->ops->write == NULL ||
    data == NULL       ||
    size == 0u
  ) {
    return 0u;
  }

  return log_h->ops->write(log_h, data, size);
}
