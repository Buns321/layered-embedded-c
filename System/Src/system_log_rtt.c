//
// 由 Buns_ 创建于 2026/10/21。
//

#include "system_log_rtt.h"
#include "SEGGER_RTT.h"
#include <assert.h>

static uint32_t Sys_Log_RTT_Write(
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
  .write = Sys_Log_RTT_Write,
};

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
