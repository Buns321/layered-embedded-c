//
// Created by Buns_ on 2026/10/21.
//

#include "system_log_base.h"
#include <assert.h>
#include <stddef.h>

void Sys_Log_Base_Init(
  sys_log_base_t *this,
  const char *name,
  const sys_log_ops_t *ops
) {
  assert(
    this        &&
    name        &&
    ops         &&
    ops->write
  );

  this->name = name;
  this->ops  = ops;
}

uint32_t Sys_Log_Base_Write(
  sys_log_base_t *this,
  const uint8_t *data,
  uint32_t size
) {
  assert(
    this         &&
    this->ops    &&
    this->ops->write &&
    data         &&
    size != 0u
  );

  if (
    this == NULL       ||
    this->ops == NULL  ||
    this->ops->write == NULL ||
    data == NULL       ||
    size == 0u
  ) {
    return 0u;
  }

  return this->ops->write(this, data, size);
}
