/**
 * @file sys_log_rtt.c
 * @brief SEGGER RTT 日志后端实现。
 * @details 属于 Layer 2（Framework/Core），通过 SEGGER_RTT_WriteNoLock 将日志文本
 *          非阻塞写入 RTT 环形缓冲区，避免无调试器连接时阻塞控制任务。
 * @ingroup Framework_Core
 */

#include "sys_log_rtt.h"
#include "SEGGER_RTT.h"
#include <assert.h>
#include <stddef.h>

/**
 * @brief RTT 后端非阻塞写入虚表实现函数。
 * @param[in,out] log_h 指向 sys_log_rtt_t 首成员的基类指针。
 * @param[in]     data  待写入的日志字节流指针。
 * @param[in]     size  待写入的字节数（单位：B）。
 * @return 实际写入 RTT 上行缓冲区的字节数（单位：B）。
 */
static uint32_t sys_log_rtt_write(sys_log_base_t *log_h, const uint8_t *data, uint32_t size) {
  assert(log_h != NULL && data != NULL && size != 0U);

  sys_log_rtt_t *rtt_h = (sys_log_rtt_t *)log_h;
  // 调用 NoLock 版本：外层 sys_log 门面已通过 try-lock 串行化写入，且紧急路径可能处于关中断态
  return (uint32_t)SEGGER_RTT_WriteNoLock(rtt_h->channel, data, size);
}

/** @brief SEGGER RTT 日志后端只读操作虚表。 */
static const sys_log_ops_t s_sys_log_ops_rtt = {
    .write = sys_log_rtt_write,
};

void Sys_Log_RTT_Init(sys_log_rtt_t *log_h, const char *name, uint32_t channel) {
  assert(log_h != NULL && name != NULL && channel < SEGGER_RTT_MAX_NUM_UP_BUFFERS);

  Sys_Log_Base_Init(&log_h->base, name, &s_sys_log_ops_rtt);
  log_h->channel = channel;

  SEGGER_RTT_Init();
  // 强制设为 NO_BLOCK_SKIP，防止宿主机未连接 J-Link RTT Viewer 时阻塞目标机实时任务
  (void)SEGGER_RTT_SetFlagsUpBuffer(channel, SEGGER_RTT_MODE_NO_BLOCK_SKIP);
}
