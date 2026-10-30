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

/**
 * @brief 初始化日志后端并绑定静态操作表。
 * @param log_h 待初始化的日志后端对象；其生命周期必须覆盖日志门面的全部使用期。
 * @param name 非空只读后端名称，生命周期必须覆盖日志门面的全部使用期。
 * @param ops 非空操作表，write 必须有效且生命周期覆盖日志门面的全部使用期。
 * @pre log_h、name、ops 均非 NULL，且 ops->write 非 NULL。
 * @post log_h 的 name 和 ops 字段已绑定；其他字段保持不变。
 * @note 不分配动态内存。函数不做并发保护，应在把对象发布给其他上下文前调用。
 * @warning Debug 构建用 assert 检查参数；Release 构建不校验 NULL，违反前置条件会解引用空指针。
 */
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
 * @param log_h 已初始化的日志后端对象。
 * @param data 待写入的只读字节缓冲区，只在本次调用期间访问。
 * @param size 待写入字节数，单位 byte，必须大于 0。
 * @retval 0 参数无效、后端不可用或后端丢弃本次数据。
 * @retval 1..size 后端接受的字节数，最大等于 size。
 * @pre log_h、log_h->ops、ops->write、data 均非 NULL，且 size > 0。
 * @post 不取得缓冲区所有权；具体写入副作用由后端 write 定义。
 * @note 不分配动态内存。门面不在本函数内加锁，后端必须满足非阻塞要求，并由调用方串行化。
 * @warning Debug 构建用 assert 检查参数；Release 构建对无效参数返回 0。
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
