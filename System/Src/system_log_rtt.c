//
// 由 Buns_ 创建于 2026/10/21。
//

#include "system_log_rtt.h"
#include "SEGGER_RTT.h"
#include <assert.h>

static uint32_t Sys_Log_RTT_Write(
  sys_log_base_t *this,
  const uint8_t *data,
  uint32_t size
) {
  assert(this && data && size != 0u);

  system_log_rtt_t *self = (system_log_rtt_t *)this;
  return (uint32_t)SEGGER_RTT_WriteNoLock(
    self->channel,
    data,
    size
  );
}

static const sys_log_ops_t sys_log_ops_rtt = {
  .write = Sys_Log_RTT_Write,
};

void Sys_Log_RTT_Init(
  system_log_rtt_t *this,
  const char *name,
  uint32_t channel
) {
  assert(
    this &&
    name &&
    (channel < SEGGER_RTT_MAX_NUM_UP_BUFFERS)
  );

  Sys_Log_Base_Init(&this->base, name, &sys_log_ops_rtt);
  this->channel = channel;

  SEGGER_RTT_Init();
  SEGGER_RTT_SetFlagsUpBuffer(channel, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}
