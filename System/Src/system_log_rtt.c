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
 * @param log_h RTT 后端嵌入的 sys_log_base_t 地址；必须来自已初始化的 system_log_rtt_t。
 * @param data 待写入的只读字节缓冲区，只在本次调用期间访问。
 * @param size 待写入字节数，单位 byte，必须大于 0。
 * @retval 0 RTT 上行缓冲区无空间或丢弃本次数据。
 * @retval 1..size SEGGER_RTT_WriteNoLock 接受的字节数，最大等于 size。
 * @pre log_h 指向已初始化的 RTT 后端，data 非 NULL，size > 0，且 channel 有效。
 * @post 不取得缓冲区所有权；数据复制到指定 RTT 上行缓冲区或按 NO_BLOCK_SKIP 模式跳过。
 * @note 私有函数，仅由 sys_log_ops_rtt 调用。使用 NoLock 版本，调用方必须保证
 *       同一 RTT 通道的写入串行化；不分配动态内存。
 * @warning Debug 构建用 assert 检查参数；Release 构建不校验 NULL 或 size。
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
 * @param log_h 待初始化的 RTT 后端对象；生命周期必须覆盖日志门面的全部使用期。
 * @param name 非空只读调试名称，生命周期必须覆盖日志门面的全部使用期。
 * @param channel RTT 上行通道号，范围 0..SEGGER_RTT_MAX_NUM_UP_BUFFERS-1，通常为 0。
 * @pre log_h 和 name 非 NULL，channel 在 SEGGER RTT 上行通道范围内。
 * @post base 已绑定 RTT 操作表和名称，channel 已保存，RTT 已初始化且该上行通道
 *       配置为 SEGGER_RTT_MODE_NO_BLOCK_SKIP。
 * @note 不分配动态内存；应在发布给其他任务或 ISR 前完成初始化。
 * @warning Debug 构建用 assert 检查参数；Release 构建不校验，非法 channel 会访问
 *          未定义的 RTT 缓冲区配置。
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
