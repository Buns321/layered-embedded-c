/**
 * @file sys_log.h
 * @brief 非阻塞 System 日志门面（单行上限 192 byte，默认时间戳来自 HAL_GetTick）。
 * @details 属于 Layer 2（Framework/Core），提供基于快速位掩码的零开销等级过滤、
 *          try-lock 非阻塞并发保护与紧急直写路径；无动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_LOG_H
#define LAYERED_CAR_SYS_LOG_H

#include "sys_log_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 日志严重等级枚举（数值越小优先级越高）。
 */
typedef enum sys_log_level_e {
  SYS_LOG_LEVEL_ERROR = 0, /**< 致命或不可恢复错误级日志。 */
  SYS_LOG_LEVEL_WARN,      /**< 可恢复异常或降级警告级日志。 */
  SYS_LOG_LEVEL_INFO,      /**< 关键状态迁移与启动信息级日志。 */
  SYS_LOG_LEVEL_DEBUG,     /**< 周期调试与诊断细节级日志。 */
  SYS_LOG_LEVEL_NONE,      /**< 关闭全部格式化日志输出。 */
} sys_log_level_t;

/** @cond SYS_LOG_INTERNAL */
extern volatile uint32_t g_sys_log_fast_mask;
/** @endcond */

/**
 * @brief 初始化日志门面并绑定后端。
 * @param[in] sink_h 静态分配的日志后端基类指针（为 NULL 时表示关闭输出）。
 * @post 丢弃字节计数与锁竞争计数清零，并同步更新快速过滤位掩码。
 */
void Sys_Log_Init(sys_log_base_t *sink_h);

/**
 * @brief 热切换当前日志后端。
 * @param[in] sink_h 新的日志后端基类指针（允许为 NULL 以静默关闭输出）。
 * @warning 不得在并发写入尚未结束时销毁或复用旧后端对象内存。
 */
void Sys_Log_SetSink(sys_log_base_t *sink_h);

/**
 * @brief 设置最低允许输出的日志等级。
 * @param[in] level 目标最低等级；超出有效枚举范围的值按 SYS_LOG_LEVEL_NONE 处理。
 */
void Sys_Log_SetLevel(sys_log_level_t level);

/**
 * @brief 获取当前最低允许输出的日志等级（默认 SYS_LOG_LEVEL_INFO）。
 * @return 当前生效的日志等级。
 */
sys_log_level_t Sys_Log_GetLevel(void);

/**
 * @brief 查询是否已配置有效日志后端。
 * @return 若已绑定非 NULL 后端返回 true，否则返回 false。
 */
bool Sys_Log_IsReady(void);

/**
 * @brief 快速判断指定等级当前是否允许输出（被过滤时不进入临界区）。
 * @param[in] level 待检查的日志等级。
 * @return 若后端就绪且 level 低于或等于当前阈值返回 true，否则返回 false。
 */
static inline bool Sys_Log_IsEnabled(sys_log_level_t level) {
  uint32_t level_index = (uint32_t)level;
  if (level_index > (uint32_t)SYS_LOG_LEVEL_DEBUG) {
    return false;
  }
  return (g_sys_log_fast_mask & (1U << level_index)) != 0U;
}

/**
 * @brief 格式化并提交一条单行日志。
 * @param[in] level  日志等级（ERROR..DEBUG）。
 * @param[in] tag    模块标签字符串（不可为 NULL）。
 * @param[in] format printf 风格格式串（不可为 NULL）。
 * @param[in] ...    与 format 对应的可变参数列表。
 * @note 仅限任务上下文调用；若 try-lock 冲突则递增竞争计数并立即丢弃本次日志。
 */
void Sys_Log_Write(sys_log_level_t level, const char *tag, const char *format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;

/**
 * @brief 绕过格式化与门面锁直写原始字节（仅用于栈溢出或 Fault 等最后兜底路径）。
 * @param[in] data 原始字节缓冲区首地址（不可为 NULL）。
 * @param[in] size 待写入字节数（单位：B，须 > 0）。
 * @warning 不使用栈缓冲也不获取 try-lock，可在关中断异常上下文安全调用。
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size);

/**
 * @brief 查询累计被后端拒绝的日志字节数。
 * @return 累计丢弃字节数（单位：B）。
 */
uint32_t Sys_Log_GetDropBytes(void);

/**
 * @brief 查询因 try-lock 锁竞争被丢弃的日志条数。
 * @return 累计竞争丢弃条数（单位：条）。
 */
uint32_t Sys_Log_GetContentionCount(void);

/**
 * @brief 提交 ERROR 级日志（被过滤时参数不求值，仅任务上下文调用）。
 * @param tag 模块标签字符串。
 * @param ... printf 格式串及可变参数。
 */
#define SYS_LOG_ERROR(tag, ...)                                                                    \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR)) {                                                  \
      Sys_Log_Write(SYS_LOG_LEVEL_ERROR, (tag), __VA_ARGS__);                                      \
    }                                                                                              \
  } while (0)

/**
 * @brief 提交 WARN 级日志（被过滤时参数不求值，仅任务上下文调用）。
 * @param tag 模块标签字符串。
 * @param ... printf 格式串及可变参数。
 */
#define SYS_LOG_WARN(tag, ...)                                                                     \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_WARN)) {                                                   \
      Sys_Log_Write(SYS_LOG_LEVEL_WARN, (tag), __VA_ARGS__);                                       \
    }                                                                                              \
  } while (0)

/**
 * @brief 提交 INFO 级日志（被过滤时参数不求值，仅任务上下文调用）。
 * @param tag 模块标签字符串。
 * @param ... printf 格式串及可变参数。
 */
#define SYS_LOG_INFO(tag, ...)                                                                     \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_INFO)) {                                                   \
      Sys_Log_Write(SYS_LOG_LEVEL_INFO, (tag), __VA_ARGS__);                                       \
    }                                                                                              \
  } while (0)

/**
 * @brief 提交 DEBUG 级日志（被过滤时参数不求值，仅任务上下文调用）。
 * @param tag 模块标签字符串。
 * @param ... printf 格式串及可变参数。
 */
#define SYS_LOG_DEBUG(tag, ...)                                                                    \
  do {                                                                                             \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_DEBUG)) {                                                  \
      Sys_Log_Write(SYS_LOG_LEVEL_DEBUG, (tag), __VA_ARGS__);                                      \
    }                                                                                              \
  } while (0)

/** @} */

#endif /* LAYERED_CAR_SYS_LOG_H */
