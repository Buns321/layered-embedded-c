//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log_rtt.c
 * @brief SEGGER RTT 日志后端实现。
 * @ingroup system
 */

#include "system_log_rtt.h"
#include "SEGGER_RTT.h"
#include <assert.h>

/**
 * @brief 通过 RTT 后端写入日志文本。
 */
static uint32_t sys_log_rtt_write(
  sys_log_base_t *log_h,
  const uint8_t *data,
  uint32_t size
) {
  assert(log_h && data && size != 0u);

  system_log_rtt_t *system_log_rtt_h = (system_log_rtt_t *)log_h;
  return (uint32_t)SEGGER_RTT_WriteNoLock(
    system_log_rtt_h->channel,
    data,
    size
  );
}

static const sys_log_ops_t sys_log_ops_rtt = {
  .write = sys_log_rtt_write,
};

/**
 * @brief 初始化 RTT 日志后端。
 * @param log_h 后端对象。
 * @param name 调试名称。
 * @param channel RTT 上行通道，通常为 0。
 */
void Sys_Log_RTT_Init(
  system_log_rtt_t *log_h,
  const char *name,
  uint32_t channel
) {
  assert(
    log_h &&
    name &&
    (channel < SEGGER_RTT_MAX_NUM_UP_BUFFERS)
  );

  Sys_Log_Base_Init(&log_h->base, name, &sys_log_ops_rtt);
  log_h->channel = channel;

  SEGGER_RTT_Init();
  SEGGER_RTT_SetFlagsUpBuffer(channel, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}
