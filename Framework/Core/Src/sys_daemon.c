/**
 * @file sys_daemon.c
 * @brief 静态离线看门狗服务实现。
 * @details 属于 Layer 2（Framework/Core），维护静态指针表 s_daemon_nodes；在临界区内
 *          原子读取喂狗时间戳与序号并推进状态机，随后退出临界区再派发边沿回调，避免
 *          用户回调内调用阻塞或重入接口引发死锁。
 * @ingroup Framework_Core
 */

#include "sys_daemon.h"
#include "sys_critical.h"
#include "sys_time_window.h"
#include <stddef.h>

/** @brief 静态看门狗节点指针注册表（固定容量 SYS_DAEMON_MAX_NODES）。 */
static sys_daemon_t *s_daemon_nodes[SYS_DAEMON_MAX_NODES];

/**
 * @brief 判断指定参考时间戳距今是否已严格超过超时阈值（抗时间回绕与 ISR 轻微超前）。
 * @param[in] now_ms     当前任务采样的单调毫秒时间戳（单位：ms）。
 * @param[in] ref_ms     参考基准时间戳（上次喂狗或注册时刻，单位：ms）。
 * @param[in] timeout_ms 允许的最大超时阈值（单位：ms，须 < 0x80000000）。
 * @return 是否已超时。
 */
static bool sys_daemon_isexpired(uint32_t now_ms, uint32_t ref_ms, uint32_t timeout_ms) {
  // FeedFromISR 若以稍晚于任务采样 now_ms 的时间戳喂狗，ref_ms 会轻微超前于 now_ms，
  // 若直接做无符号减法会下溢成接近 0xFFFFFFFF 的大数而被误判超时，故先检查 now_ms >= ref_ms。
  if (!Sys_Time_HasReachedMs(now_ms, ref_ms)) {
    return false;
  }
  return Sys_Time_IsExpiredMs(now_ms, ref_ms, timeout_ms);
}

void Sys_Daemon_ResetAll(void) {
  Sys_Critical_Enter();
  for (uint32_t i = 0U; i < SYS_DAEMON_MAX_NODES; ++i) {
    s_daemon_nodes[i] = NULL;
  }
  Sys_Critical_Exit();
}

bool Sys_Daemon_Register(sys_daemon_t *node, const sys_daemon_cfg_t *cfg, uint32_t now_ms) {
  // 限制 timeout_ms 与 init_grace_ms 在 32 位半周期（< 0x80000000）内，保证回绕比较无歧义
  if (node == NULL || cfg == NULL || cfg->timeout_ms == 0U || cfg->timeout_ms >= 0x80000000U ||
      cfg->init_grace_ms >= 0x80000000U) {
    return false;
  }

  Sys_Critical_Enter();
  uint32_t free_idx = SYS_DAEMON_MAX_NODES;
  for (uint32_t i = 0U; i < SYS_DAEMON_MAX_NODES; ++i) {
    // 禁止同一静态实例重复注册到多个槽位
    if (s_daemon_nodes[i] == node) {
      Sys_Critical_Exit();
      return false;
    }
    if (s_daemon_nodes[i] == NULL && free_idx == SYS_DAEMON_MAX_NODES) {
      free_idx = i;
    }
  }
  if (free_idx >= SYS_DAEMON_MAX_NODES) {
    Sys_Critical_Exit();
    return false;
  }

  *node = (sys_daemon_t){.registered = false};
  node->cfg = *cfg;
  // 未显式配置启动宽限期时，默认与运行期 timeout_ms 保持一致
  if (node->cfg.init_grace_ms == 0U) {
    node->cfg.init_grace_ms = cfg->timeout_ms;
  }
  node->registered_ms = now_ms;
  node->last_feed_ms = now_ms;
  node->state = SYS_DAEMON_INIT_GRACE;
  node->registered = true;
  s_daemon_nodes[free_idx] = node;
  Sys_Critical_Exit();

  return true;
}

void Sys_Daemon_Feed(sys_daemon_t *node, uint32_t now_ms) {
  if (node == NULL || !node->registered) {
    return;
  }

  // 在临界区内成对更新时间戳与序号，防止与 Poll 读取时产生撕裂
  Sys_Critical_Enter();
  node->last_feed_ms = now_ms;
  node->feed_seq++;
  Sys_Critical_Exit();
}

void Sys_Daemon_FeedFromISR(sys_daemon_t *node, uint32_t now_ms) {
  if (node == NULL || !node->registered) {
    return;
  }

  // ISR 中仅更新时间戳与序号，将状态迁移与回调推迟到任务上下文的 Sys_Daemon_Poll 执行
  sys_critical_state_t critical_state = Sys_Critical_EnterFromISR();
  node->last_feed_ms = now_ms;
  node->feed_seq++;
  Sys_Critical_ExitFromISR(critical_state);
}

void Sys_Daemon_Poll(uint32_t now_ms) {
  for (uint32_t i = 0U; i < SYS_DAEMON_MAX_NODES; ++i) {
    sys_daemon_cb_t cb = NULL;
    void *cb_ctx = NULL;

    // 临界区内仅做快照采样与状态迁移决策，记录待触发的边沿回调指针
    Sys_Critical_Enter();
    sys_daemon_t *node = s_daemon_nodes[i];
    if (node != NULL && node->registered) {
      uint32_t last_feed_ms = node->last_feed_ms;
      uint32_t feed_seq = node->feed_seq;
      bool has_new_feed = (feed_seq != node->seen_feed_seq);
      node->seen_feed_seq = feed_seq;

      switch (node->state) {
      case SYS_DAEMON_INIT_GRACE:
        if (has_new_feed) {
          // 宽限期内收到喂狗：若该喂狗本身未过期则转入 ONLINE，否则直接判 OFFLINE
          if (sys_daemon_isexpired(now_ms, last_feed_ms, node->cfg.timeout_ms)) {
            node->state = SYS_DAEMON_OFFLINE;
            node->offline_count++;
            cb = node->cfg.on_offline;
            cb_ctx = node->cfg.ctx;
          } else {
            node->state = SYS_DAEMON_ONLINE;
          }
        } else if (sys_daemon_isexpired(now_ms, node->registered_ms, node->cfg.init_grace_ms)) {
          // 宽限期满仍未收到任何喂狗，触发首次离线边沿回调
          node->state = SYS_DAEMON_OFFLINE;
          node->offline_count++;
          cb = node->cfg.on_offline;
          cb_ctx = node->cfg.ctx;
        }
        break;

      case SYS_DAEMON_ONLINE:
        if (sys_daemon_isexpired(now_ms, last_feed_ms, node->cfg.timeout_ms)) {
          node->state = SYS_DAEMON_OFFLINE;
          node->offline_count++;
          cb = node->cfg.on_offline;
          cb_ctx = node->cfg.ctx;
        }
        break;

      case SYS_DAEMON_OFFLINE:
      default:
        // 离线态仅在收到新喂狗且当前仍处于有效期内时恢复至 ONLINE，并触发恢复边沿回调
        if (has_new_feed && !sys_daemon_isexpired(now_ms, last_feed_ms, node->cfg.timeout_ms)) {
          node->state = SYS_DAEMON_ONLINE;
          cb = node->cfg.on_recover;
          cb_ctx = node->cfg.ctx;
        }
        break;
      }
    }
    Sys_Critical_Exit();

    // 在临界区外同步执行边沿回调，避免关中断期间执行日志/状态切换等复杂操作
    if (cb != NULL) {
      cb(cb_ctx, now_ms);
    }
  }
}

sys_daemon_state_t Sys_Daemon_GetState(const sys_daemon_t *node) {
  if (node == NULL || !node->registered) {
    return SYS_DAEMON_OFFLINE;
  }
  return node->state;
}

bool Sys_Daemon_IsOnline(const sys_daemon_t *node) {
  return Sys_Daemon_GetState(node) == SYS_DAEMON_ONLINE;
}
