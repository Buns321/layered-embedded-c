/**
 * @file sys_log_base.c
 * @brief 日志后端抽象基类实现。
 * @details 属于 Layer 2（Framework/Core），完成日志后端基类字段绑定与参数防呆校验后的虚表转发。
 * @ingroup Framework_Core
 */

#include "sys_log_base.h"
#include <assert.h>
#include <stddef.h>

void Sys_Log_Base_Init(sys_log_base_t *log_h, const char *name, const sys_log_ops_t *ops) {
  assert(log_h != NULL && name != NULL && ops != NULL && ops->write != NULL);

  log_h->name = name;
  log_h->ops = ops;
}

uint32_t Sys_Log_Base_Write(sys_log_base_t *log_h, const uint8_t *data, uint32_t size) {
  assert(log_h != NULL && log_h->ops != NULL && log_h->ops->write != NULL && data != NULL &&
         size != 0U);

  // Release 构建（NDEBUG）下保留运行时空指针与零长度兜底，防止异常路径触发空解引用
  if (log_h == NULL || log_h->ops == NULL || log_h->ops->write == NULL || data == NULL ||
      size == 0U) {
    return 0U;
  }

  return log_h->ops->write(log_h, data, size);
}
