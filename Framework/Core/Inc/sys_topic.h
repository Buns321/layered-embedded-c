/**
 * @file sys_topic.h
 * @brief 严格 ISO C11 静态类型安全 Pub-Sub 通道宏。
 * @addtogroup sys_core
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
  uint32_t time_ms;
  uint32_t sequence;
  bool valid;
} sys_topic_meta_t;

/**
 * @brief 声明强类型 Topic 通道及内联 Publish / Read / Reset 访问函数（仅任务上下文调用）。
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
 */
#define SYS_TOPIC_DEFINE(name)                                                                     \
  name##_topic_t g_topic_##name = {.meta = {.time_ms = 0U, .sequence = 0U, .valid = false}}

/** @} */

#endif /* LAYERED_CAR_SYS_TOPIC_H */
