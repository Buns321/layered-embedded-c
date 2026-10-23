//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log_base.h
 * @brief System 日志输出后端接口。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_BASE_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_BASE_H

#include <stdint.h>

typedef struct sys_log_base sys_log_base_t;

/**
 * @brief 日志后端操作表。
 * @note write 必须在数据失效前返回，不得阻塞，并返回已接受的字节数；
 *       返回 0 表示丢弃该条日志。
 */
typedef struct sys_log_ops {
  uint32_t (*write)(
    sys_log_base_t *this,
    const uint8_t *data,
    uint32_t size
  );
} sys_log_ops_t;

/**
 * @brief 日志后端抽象对象。
 */
typedef struct sys_log_base {
  const sys_log_ops_t *ops;
  const char          *name;
} sys_log_base_t;

/** @brief 初始化日志后端并绑定静态操作表。 */
void Sys_Log_Base_Init(
  sys_log_base_t *this,
  const char *name,
  const sys_log_ops_t *ops
);

/**
 * @brief 向日志后端写入字节。
 * @param this 后端对象。
 * @param data 字节缓冲区。
 * @param size 字节数。
 * @return 已接受的字节数；后端不可用或数据被丢弃时返回 0。
 */
uint32_t Sys_Log_Base_Write(
  sys_log_base_t *this,
  const uint8_t *data,
  uint32_t size
);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_BASE_H
