/**
 * @file sys_daemon.c
 * @brief 静态离线看门狗服务实现。
 * @ingroup sys_core
 */

#include "sys_daemon.h"
#include "sys_critical.h"
#include "sys_time_window.h"
#include <stddef.h>

static sys_daemon_t *s_daemon_nodes[SYS_DAEMON_MAX_NODES];

static bool sys_daemon_isexpired(uint32_t now_ms, uint32_t ref_ms, uint32_t timeout_ms) {
  /* FeedFromISR 若以稍晚于任务采样 now_ms 的时间戳喂狗，ref_ms 会超前，不得判超时。 */
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
  if (node == NULL || cfg == NULL || cfg->timeout_ms == 0U || cfg->timeout_ms >= 0x80000000U ||
      cfg->init_grace_ms >= 0x80000000U) {
    return false;
  }

  Sys_Critical_Enter();
  uint32_t free_idx = SYS_DAEMON_MAX_NODES;
  for (uint32_t i = 0U; i < SYS_DAEMON_MAX_NODES; ++i) {
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

  Sys_Critical_Enter();
  node->last_feed_ms = now_ms;
  node->feed_seq++;
  Sys_Critical_Exit();
}

void Sys_Daemon_FeedFromISR(sys_daemon_t *node, uint32_t now_ms) {
  if (node == NULL || !node->registered) {
    return;
  }

  sys_critical_state_t critical_state = Sys_Critical_EnterFromISR();
  node->last_feed_ms = now_ms;
  node->feed_seq++;
  Sys_Critical_ExitFromISR(critical_state);
}

void Sys_Daemon_Poll(uint32_t now_ms) {
  for (uint32_t i = 0U; i < SYS_DAEMON_MAX_NODES; ++i) {
    sys_daemon_cb_t cb = NULL;
    void *cb_ctx = NULL;

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
          if (sys_daemon_isexpired(now_ms, last_feed_ms, node->cfg.timeout_ms)) {
            node->state = SYS_DAEMON_OFFLINE;
            node->offline_count++;
            cb = node->cfg.on_offline;
            cb_ctx = node->cfg.ctx;
          } else {
            node->state = SYS_DAEMON_ONLINE;
          }
        } else if (sys_daemon_isexpired(now_ms, node->registered_ms, node->cfg.init_grace_ms)) {
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
        if (has_new_feed && !sys_daemon_isexpired(now_ms, last_feed_ms, node->cfg.timeout_ms)) {
          node->state = SYS_DAEMON_ONLINE;
          cb = node->cfg.on_recover;
          cb_ctx = node->cfg.ctx;
        }
        break;
      }
    }
    Sys_Critical_Exit();

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
