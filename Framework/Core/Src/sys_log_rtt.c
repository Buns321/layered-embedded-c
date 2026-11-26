/**
 * @file sys_log_rtt.c
 * @brief SEGGER RTT 日志后端实现。
 * @ingroup sys_core
 */

#include "sys_log_rtt.h"
#include "SEGGER_RTT.h"
#include <assert.h>
#include <stddef.h>

static uint32_t sys_log_rtt_write(sys_log_base_t *log_h, const uint8_t *data, uint32_t size) {
  assert(log_h != NULL && data != NULL && size != 0U);

  sys_log_rtt_t *rtt_h = (sys_log_rtt_t *)log_h;
  return (uint32_t)SEGGER_RTT_WriteNoLock(rtt_h->channel, data, size);
}

static const sys_log_ops_t s_sys_log_ops_rtt = {
    .write = sys_log_rtt_write,
};

void Sys_Log_RTT_Init(sys_log_rtt_t *log_h, const char *name, uint32_t channel) {
  assert(log_h != NULL && name != NULL && channel < SEGGER_RTT_MAX_NUM_UP_BUFFERS);

  Sys_Log_Base_Init(&log_h->base, name, &s_sys_log_ops_rtt);
  log_h->channel = channel;

  SEGGER_RTT_Init();
  (void)SEGGER_RTT_SetFlagsUpBuffer(channel, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}
