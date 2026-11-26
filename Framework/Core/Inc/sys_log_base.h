/**
 * @file sys_log_base.h
 * @brief System 日志输出后端抽象接口。
 * @details 属于 Layer 2（Framework/Core），定义日志后端的只读操作表（sys_log_ops_t）与
 *          抽象基类（sys_log_base_t），供 sys_log 门面以统一方式向 RTT/UART 等后端输出。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_LOG_BASE_H
#define LAYERED_CAR_SYS_LOG_BASE_H

#include <stdint.h>

/** @brief 日志后端抽象基类前向声明。 */
typedef struct sys_log_base_s sys_log_base_t;

/**
 * @brief 日志后端操作表（只读虚表）。
 */
typedef struct sys_log_ops_s {
  /**
   * @brief 非阻塞写入日志字节流。
   * @param[in,out] log_h 指向具体日志后端实例的基类指针。
   * @param[in]     data  待写入的字节缓冲区首地址。
   * @param[in]     size  待写入的字节数（单位：B）。
   * @return 实际被后端接受的字节数（单位：B；0 表示全部丢弃）。
   */
  uint32_t (*write)(sys_log_base_t *log_h, const uint8_t *data, uint32_t size);
} sys_log_ops_t;

/**
 * @brief 日志后端抽象基类（须作为具体后端结构体的首成员以支持安全基类指针转换）。
 */
typedef struct sys_log_base_s {
  const sys_log_ops_t *ops; /**< 指向后端只读操作表的指针（不可为 NULL）。 */
  const char *name;         /**< 后端只读名称字符串（用于调试识别，不可为 NULL）。 */
} sys_log_base_t;

/**
 * @brief 初始化日志后端基类并绑定静态名称与操作表。
 * @param[out] log_h 日志后端基类指针（不可为 NULL）。
 * @param[in]  name  后端名称字符串（不可为 NULL，生命周期须覆盖后端对象）。
 * @param[in]  ops   只读操作表指针（不可为 NULL，且 ops->write 不可为 NULL）。
 */
void Sys_Log_Base_Init(sys_log_base_t *log_h, const char *name, const sys_log_ops_t *ops);

/**
 * @brief 向日志后端非阻塞写入字节（不获取缓冲区所有权）。
 * @param[in,out] log_h 日志后端基类指针（不可为 NULL）。
 * @param[in]     data  待写入的字节缓冲区首地址（不可为 NULL）。
 * @param[in]     size  待写入字节数（单位：B，须 > 0）。
 * @return 实际被后端接受的字节数（单位：B；参数非法时返回 0）。
 */
uint32_t Sys_Log_Base_Write(sys_log_base_t *log_h, const uint8_t *data, uint32_t size);

/** @} */

#endif /* LAYERED_CAR_SYS_LOG_BASE_H */
