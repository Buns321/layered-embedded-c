/**
 * @file sys_daemon.h
 * @brief 静态离线看门狗服务：基于单调毫秒时间戳监测节点在线与离线边沿。
 * @details 属于 Layer 2（Framework/Core），通过固定上限节点槽位（SYS_DAEMON_MAX_NODES）
 *          静态管理外设或任务心跳；支持 ISR/任务双端喂狗，并在任务上下文轮询触发离线/恢复边沿回调。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_DAEMON_H
#define LAYERED_CAR_SYS_DAEMON_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 静态看门狗注册表最大节点容量（单位：个）。 */
#ifndef SYS_DAEMON_MAX_NODES
#define SYS_DAEMON_MAX_NODES 16U
#endif

/**
 * @brief 看门狗节点在线状态枚举。
 */
typedef enum sys_daemon_state_e {
  SYS_DAEMON_INIT_GRACE = 0U, /**< 注册后初始宽限期，尚未收到首次喂狗且未超宽限时间。 */
  SYS_DAEMON_ONLINE,          /**< 节点已收到喂狗且距今未超过 timeout_ms。 */
  SYS_DAEMON_OFFLINE,         /**< 节点超过宽限期或喂狗超时，处于离线状态。 */
} sys_daemon_state_t;

/**
 * @brief 节点离线或恢复边沿回调签名。
 * @param[in,out] ctx    注册时透传的用户上下文指针。
 * @param[in]     now_ms 触发边沿迁移时的系统单调毫秒时间戳（单位：ms）。
 * @note 在临界区外由 Sys_Daemon_Poll() 同步触发，可安全执行状态切换或日志记录。
 */
typedef void (*sys_daemon_cb_t)(void *ctx, uint32_t now_ms);

/**
 * @brief 静态看门狗节点注册配置。
 */
typedef struct sys_daemon_cfg_s {
  const char *name;       /**< 节点名称字符串（只读，用于调试识别，允许为 NULL）。 */
  uint32_t timeout_ms;    /**< 运行期喂狗超时阈值（单位：ms，合法范围 1..0x7FFFFFFF）。 */
  uint32_t init_grace_ms; /**< 注册后首次喂狗宽限期（单位：ms，为 0 时默认取 timeout_ms）。 */
  sys_daemon_cb_t
      on_offline; /**< 由 INIT_GRACE/ONLINE 跌落至 OFFLINE 时的边沿回调（可为 NULL）。 */
  sys_daemon_cb_t on_recover; /**< 由 OFFLINE 恢复至 ONLINE 时的边沿回调（可为 NULL）。 */
  void *ctx;                  /**< 透传给边沿回调的用户上下文指针（可为 NULL）。 */
} sys_daemon_cfg_t;

/**
 * @brief 静态看门狗节点实例（由调用方静态分配，生命周期须覆盖监控期）。
 */
typedef struct sys_daemon_s {
  sys_daemon_cfg_t cfg;              /**< 节点配置副本。 */
  volatile uint32_t last_feed_ms;    /**< 最近一次喂狗的单调毫秒时间戳（单位：ms）。 */
  volatile uint32_t feed_seq;        /**< 喂狗递增序号，用于无锁/临界区外检测新喂狗事件。 */
  uint32_t registered_ms;            /**< 节点注册时刻的单调毫秒时间戳（单位：ms）。 */
  uint32_t seen_feed_seq;            /**< Poll() 上次已观测到的喂狗序号。 */
  uint32_t offline_count;            /**< 累计跌落至离线状态的次数（单位：次）。 */
  volatile sys_daemon_state_t state; /**< 节点当前状态（INIT_GRACE / ONLINE / OFFLINE）。 */
  bool registered;                   /**< 节点是否已成功注册进全局表。 */
} sys_daemon_t;

/**
 * @brief 清空所有已注册看门狗槽位。
 * @note 仅在系统初始化或宿主机单元测试重置阶段调用。
 */
void Sys_Daemon_ResetAll(void);

/**
 * @brief 注册静态看门狗节点并进入 INIT_GRACE 状态。
 * @param[in,out] node   调用方静态分配的节点实例指针（不可为 NULL，且不可重复注册）。
 * @param[in]     cfg    节点超时与回调配置指针（不可为 NULL；timeout_ms 须在 1..0x7FFFFFFF）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @return 注册成功与否。
 * @retval true  节点已写入空闲槽位并启动宽限期监控。
 * @retval false 参数非法、节点已注册或注册表已满（SYS_DAEMON_MAX_NODES）。
 */
bool Sys_Daemon_Register(sys_daemon_t *node, const sys_daemon_cfg_t *cfg, uint32_t now_ms);

/**
 * @brief 任务上下文喂狗（仅原子更新时间戳与序号，不执行回调）。
 * @param[in,out] node   已注册的看门狗节点指针（NULL 或未注册时安全忽略）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 仅限任务上下文调用；ISR 请使用 Sys_Daemon_FeedFromISR()。
 */
void Sys_Daemon_Feed(sys_daemon_t *node, uint32_t now_ms);

/**
 * @brief ISR 上下文喂狗（仅原子更新时间戳与序号，不执行回调）。
 * @param[in,out] node   已注册的看门狗节点指针（NULL 或未注册时安全忽略）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @note 仅限中断服务程序上下文调用，内部使用 Sys_Critical_EnterFromISR/ExitFromISR。
 */
void Sys_Daemon_FeedFromISR(sys_daemon_t *node, uint32_t now_ms);

/**
 * @brief 周期轮询所有已注册节点并推进状态迁移与边沿回调。
 * @param[in] now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 仅限任务上下文调用；边沿回调将在临界区外同步执行，避免关中断期间执行耗时逻辑。
 */
void Sys_Daemon_Poll(uint32_t now_ms);

/**
 * @brief 查询节点当前是否处于 SYS_DAEMON_ONLINE 状态。
 * @param[in] node 目标看门狗节点指针（允许为 NULL）。
 * @return 节点是否在线。
 * @retval true  节点已注册且处于 SYS_DAEMON_ONLINE。
 * @retval false 节点为 NULL、未注册、处于宽限期或已离线。
 */
bool Sys_Daemon_IsOnline(const sys_daemon_t *node);

/**
 * @brief 读取节点当前状态。
 * @param[in] node 目标看门狗节点指针（允许为 NULL）。
 * @return 节点当前状态；若 node 为 NULL 或未注册，返回 SYS_DAEMON_OFFLINE。
 */
sys_daemon_state_t Sys_Daemon_GetState(const sys_daemon_t *node);

/** @} */

#endif /* LAYERED_CAR_SYS_DAEMON_H */
