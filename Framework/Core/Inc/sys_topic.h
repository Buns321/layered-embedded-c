/**
 * @file sys_topic.h
 * @brief 严格 ISO C11 静态类型安全 Pub-Sub 通道宏。
 * @details 属于 Layer 2（Framework/Core），通过宏生成强类型全局 Topic 存储实例与内联
 *          Publish/Read/Reset 访问函数，内部由 Sys_Critical_Enter/Exit 保证载荷与元数据
 *          的跨任务一致性快照；不使用 GNU 语句表达式等非标准扩展，零动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_TOPIC_H
#define LAYERED_CAR_SYS_TOPIC_H

#include "sys_critical.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Topic 元数据快照（记录发布时间戳、递增序号与有效标志）。
 */
typedef struct sys_topic_meta_s {
  uint32_t time_ms;  /**< 最近一次发布时的系统单调毫秒时间戳（单位：ms）。 */
  uint32_t sequence; /**< 累计发布递增序号，用于消费者判断数据是否更新（单位：次）。 */
  bool valid;        /**< 是否已至少成功发布过一次有效载荷。 */
} sys_topic_meta_t;

/**
 * @brief 声明强类型 Topic 通道及内联 Publish / Read / Reset 访问函数。
 * @param name      Topic 逻辑名称标识符（将生成 `<name>_topic_t`、`g_topic_<name>` 与
 * `Topic_<name>_*`）。
 * @param payload_t Topic 载荷数据类型（须为可赋值聚合或标量类型）。
 * @note 生成的 Topic_<name>_Publish / Read / Reset 仅限任务上下文调用（内部使用任务临界区）。
 */
#define SYS_TOPIC_DECLARE(name, payload_t)                                                         \
  typedef struct name##_topic_s {                                                                  \
    payload_t payload;                                                                             \
    sys_topic_meta_t meta;                                                                         \
  } name##_topic_t;                                                                                \
  extern name##_topic_t g_topic_##name;                                                            \
  static inline void Topic_##name##_Publish(const payload_t *p, uint32_t now_ms) {                 \
    if (p == NULL) {                                                                               \
      return;                                                                                      \
    }                                                                                              \
    Sys_Critical_Enter();                                                                          \
    g_topic_##name.payload = *p;                                                                   \
    g_topic_##name.meta.time_ms = now_ms;                                                          \
    g_topic_##name.meta.sequence++;                                                                \
    g_topic_##name.meta.valid = true;                                                              \
    Sys_Critical_Exit();                                                                           \
  }                                                                                                \
  static inline bool Topic_##name##_Read(payload_t *out, sys_topic_meta_t *meta) {                 \
    sys_topic_meta_t snap_meta;                                                                    \
    Sys_Critical_Enter();                                                                          \
    if (out != NULL) {                                                                             \
      *out = g_topic_##name.payload;                                                               \
    }                                                                                              \
    snap_meta = g_topic_##name.meta;                                                               \
    Sys_Critical_Exit();                                                                           \
    if (meta != NULL) {                                                                            \
      *meta = snap_meta;                                                                           \
    }                                                                                              \
    return snap_meta.valid;                                                                        \
  }                                                                                                \
  static inline void Topic_##name##_Reset(void) {                                                  \
    const name##_topic_t zero = {.meta = {.time_ms = 0U, .sequence = 0U, .valid = false}};         \
    Sys_Critical_Enter();                                                                          \
    g_topic_##name = zero;                                                                         \
    Sys_Critical_Exit();                                                                           \
  }                                                                                                \
  extern name##_topic_t g_topic_##name

/**
 * @brief 定义并零初始化静态 Topic 通道存储实例。
 * @param name 与对应 SYS_TOPIC_DECLARE(name, payload_t) 一致的 Topic 逻辑名称。
 */
#define SYS_TOPIC_DEFINE(name)                                                                     \
  name##_topic_t g_topic_##name = {.meta = {.time_ms = 0U, .sequence = 0U, .valid = false}}

/** @} */

#endif /* LAYERED_CAR_SYS_TOPIC_H */
