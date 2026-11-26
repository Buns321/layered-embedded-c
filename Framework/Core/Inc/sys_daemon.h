/**
 * @file sys_daemon.h
 * @brief 静态离线看门狗服务：基于单调毫秒时间戳监测节点在线与离线边沿。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_DAEMON_H
#define LAYERED_CAR_SYS_DAEMON_H

#include <stdbool.h>
#include <stdint.h>

#ifndef SYS_DAEMON_MAX_NODES
#define SYS_DAEMON_MAX_NODES 16U
#endif

/**
 * @brief 节点在线状态。
 */
typedef enum sys_daemon_state_e {
  SYS_DAEMON_INIT_GRACE = 0U,
  SYS_DAEMON_ONLINE,
  SYS_DAEMON_OFFLINE,
} sys_daemon_state_t;

/**
 * @brief 节点离线或恢复边沿回调签名（在临界区外由 Sys_Daemon_Poll 同步触发）。
 */
typedef void (*sys_daemon_cb_t)(void *ctx, uint32_t now_ms);

/**
 * @brief 节点注册配置。
 */
typedef struct sys_daemon_cfg_s {
  const char *name;
  uint32_t timeout_ms;
  uint32_t init_grace_ms;
  sys_daemon_cb_t on_offline;
  sys_daemon_cb_t on_recover;
  void *ctx;
} sys_daemon_cfg_t;

/**
 * @brief 静态看门狗节点实例（由调用方静态分配，生命周期覆盖监控期）。
 */
typedef struct sys_daemon_s {
  sys_daemon_cfg_t cfg;
  volatile uint32_t last_feed_ms;
  volatile uint32_t feed_seq;
  uint32_t registered_ms;
  uint32_t seen_feed_seq;
  uint32_t offline_count;
  volatile sys_daemon_state_t state;
  bool registered;
} sys_daemon_t;

/**
 * @brief 清空所有已注册看门狗槽位（仅初始化或宿主机单元测试重置调用）。
 */
void Sys_Daemon_ResetAll(void);

/**
 * @brief 注册静态看门狗节点并进入 INIT_GRACE 状态（timeout_ms 须在 1..0x7FFFFFFF）。
 */
bool Sys_Daemon_Register(sys_daemon_t *node, const sys_daemon_cfg_t *cfg, uint32_t now_ms);

/**
 * @brief 任务上下文喂狗（仅原子更新时间戳与序号，不执行回调）。
 */
void Sys_Daemon_Feed(sys_daemon_t *node, uint32_t now_ms);

/**
 * @brief ISR 上下文喂狗（仅原子更新时间戳与序号，不执行回调）。
 */
void Sys_Daemon_FeedFromISR(sys_daemon_t *node, uint32_t now_ms);

/**
 * @brief 周期轮询所有已注册节点并推进状态迁移与边沿回调（仅任务上下文调用）。
 */
void Sys_Daemon_Poll(uint32_t now_ms);

/**
 * @brief 查询节点当前是否处于 SYS_DAEMON_ONLINE 状态。
 */
bool Sys_Daemon_IsOnline(const sys_daemon_t *node);

/**
 * @brief 读取节点当前状态；NULL 或未注册节点返回 SYS_DAEMON_OFFLINE。
 */
sys_daemon_state_t Sys_Daemon_GetState(const sys_daemon_t *node);

/** @} */

#endif /* LAYERED_CAR_SYS_DAEMON_H */
