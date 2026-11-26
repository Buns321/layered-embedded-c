/**
 * @file sys_log_base.h
 * @brief System 日志输出后端抽象接口。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_LOG_BASE_H
#define LAYERED_CAR_SYS_LOG_BASE_H

#include <stdint.h>

typedef struct sys_log_base_s sys_log_base_t;

/**
 * @brief 日志后端操作表；write 须非阻塞，返回已接受字节数（0 表示丢弃）。
 */
typedef struct sys_log_ops_s {
  uint32_t (*write)(sys_log_base_t *log_h, const uint8_t *data, uint32_t size);
} sys_log_ops_t;

/**
 * @brief 日志后端抽象基类（须作为具体后端首成员）。
 */
typedef struct sys_log_base_s {
  const sys_log_ops_t *ops;
  const char *name;
} sys_log_base_t;

/**
 * @brief 初始化日志后端基类并绑定静态名称与操作表。
 */
void Sys_Log_Base_Init(sys_log_base_t *log_h, const char *name, const sys_log_ops_t *ops);

/**
 * @brief 向日志后端写入字节（不获取缓冲区所有权）。
 */
uint32_t Sys_Log_Base_Write(sys_log_base_t *log_h, const uint8_t *data, uint32_t size);

/** @} */

#endif /* LAYERED_CAR_SYS_LOG_BASE_H */
