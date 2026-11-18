/**
 * @file platform_callback_internal.h
 * @brief Platform 实现共用的单所有者回调槽注册辅助函数。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_CALLBACK_INTERNAL_H
#define LAYERED_EMBEDDED_C_PLATFORM_CALLBACK_INTERNAL_H

#include "platform_def.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 向单所有者回调槽写入一次绑定信息。
 * @param registered 该槽位对应的已注册标志指针，不得为 NULL。
 * @param slot 目标回调绑定存储指针，不得为 NULL。
 * @param binding 源回调绑定指针，不得为 NULL。
 * @param binding_size_bytes 绑定结构体字节大小，单位 byte；必须大于 0。
 * @retval PLATFORM_STATUS_OK 首次注册成功，绑定已复制且 *registered 置 true。
 * @retval PLATFORM_STATUS_BUSY *registered 已为 true，原绑定保持不变。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 存在空指针或尺寸为 0。
 * @pre 应在对应外设与中断使能前于初始化阶段调用。
 * @post 成功时 slot 获得 binding 副本并置位 *registered。
 * @note 不访问硬件、不分配内存。
 */
static inline platform_status_t platform_callback_register_slot(bool *registered, void *slot,
                                                                const void *binding,
                                                                size_t binding_size_bytes) {
  if (registered == NULL || slot == NULL || binding == NULL || binding_size_bytes == 0u) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (*registered) {
    return PLATFORM_STATUS_BUSY;
  }

  (void)memcpy(slot, binding, binding_size_bytes);
  *registered = true;
  return PLATFORM_STATUS_OK;
}

#endif // LAYERED_EMBEDDED_C_PLATFORM_CALLBACK_INTERNAL_H
