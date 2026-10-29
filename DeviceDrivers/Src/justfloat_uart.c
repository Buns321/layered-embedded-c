//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file justfloat_uart.c
 * @brief JustFloat UART 遥测协议实现。
 * @ingroup drivers_concrete
 */

#include "justfloat_uart.h"
#include <assert.h>
#include <math.h>
#include <string.h>

static const uint8_t s_justfloat_tail[JUSTFLOAT_FRAME_TAIL_SIZE] = {
  0x00u, 0x00u, 0x80u, 0x7Fu,
};

/**
 * @brief 发送完成后释放忙状态并通知完成。
 */
static void dev_justfloat_uart_ontxcplt(
  uart_base_t *uart_h,
  void *context
) {
  (void)uart_h;
  assert(context);

  justfloat_uart_t *justfloat_uart_h = (justfloat_uart_t *)context;
  justfloat_uart_h->sent_frames++;
  justfloat_uart_h->busy = false;
}

/**
 * @brief 以小端序写入 IEEE-754 浮点值。
 */
static void dev_justfloat_uart_storefloat(
  uint8_t *destination,
  float value
) {
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));

  destination[0] = (uint8_t)(bits & 0xFFu);
  destination[1] = (uint8_t)((bits >> 8) & 0xFFu);
  destination[2] = (uint8_t)((bits >> 16) & 0xFFu);
  destination[3] = (uint8_t)((bits >> 24) & 0xFFu);
}

/**
 * @brief 把遥测样本编码为 JustFloat 帧并发送。
 */
static dev_telemetry_status_t dev_justfloat_uart_send(
  telemetry_base_t *telemetry_h,
  const telemetry_sample_t *sample
) {
  assert(telemetry_h && sample);

  justfloat_uart_t *justfloat_uart_h = (justfloat_uart_t *)telemetry_h;

  if (justfloat_uart_h->busy) {
    justfloat_uart_h->dropped_frames++;
    return DEV_TELEMETRY_BUSY;
  }

  if (
    justfloat_uart_h->channel_count == 0u ||
    justfloat_uart_h->channel_count > TELEMETRY_CHANNEL_CAPACITY
  ) {
    justfloat_uart_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  for (uint8_t channel = 0u; channel < justfloat_uart_h->channel_count; channel++) {
    uint32_t mask = (uint32_t)1u << channel;
    float value = (sample->valid_mask & mask) != 0u
      ? sample->value[channel]
      : NAN;

    dev_justfloat_uart_storefloat(
      &justfloat_uart_h->tx_buffer[(uint32_t)channel * 4u],
      value
    );
  }

  uint32_t payload_size = (uint32_t)justfloat_uart_h->channel_count * 4u;
  memcpy(
    &justfloat_uart_h->tx_buffer[payload_size],
    s_justfloat_tail,
    JUSTFLOAT_FRAME_TAIL_SIZE
  );

  uint32_t frame_size = payload_size + JUSTFLOAT_FRAME_TAIL_SIZE;

  justfloat_uart_h->busy = true;
  dev_uart_status_t status = Dev_UART_TransmitDMA(
    justfloat_uart_h->uart_h,
    justfloat_uart_h->tx_buffer,
    (uint16_t)frame_size
  );

  if (status != DEV_UART_OK) {
    justfloat_uart_h->busy = false;

    if (status == DEV_UART_BUSY) {
      justfloat_uart_h->dropped_frames++;
      return DEV_TELEMETRY_BUSY;
    }

    justfloat_uart_h->error_count++;
    return DEV_TELEMETRY_ERROR;
  }

  return DEV_TELEMETRY_OK;
}

/**
 * @brief 复制遥测发送状态快照。
 */
static void dev_justfloat_uart_getstatus(
  telemetry_base_t *telemetry_h,
  telemetry_status_t *status
) {
  assert(telemetry_h && status);

  justfloat_uart_t *justfloat_uart_h = (justfloat_uart_t *)telemetry_h;
  *status = (telemetry_status_t){
    .is_busy = justfloat_uart_h->busy,
    .sent_frames = justfloat_uart_h->sent_frames,
    .dropped_frames = justfloat_uart_h->dropped_frames,
    .error_count = justfloat_uart_h->error_count,
  };
}

static const telemetry_ops_t telemetry_ops_justfloat = {
  .send       = dev_justfloat_uart_send,
  .get_status = dev_justfloat_uart_getstatus,
};

/**
 * @brief 在 UART 上初始化 JustFloat。
 * @param telemetry_h 对象。
 * @param name 调试名称。
 * @param uart_h 已完成初始化且 TX 支持 DMA 的 UART 后端。
 * @param channel_count 每帧通道数，范围 1..capacity。
 */
void Dev_JustFloat_UART_Init(
  justfloat_uart_t *telemetry_h,
  const char *name,
  uart_base_t *uart_h,
  uint8_t channel_count
) {
  assert(
    telemetry_h &&
    name &&
    uart_h &&
    channel_count > 0u &&
    channel_count <= TELEMETRY_CHANNEL_CAPACITY
  );

  Dev_Telemetry_Base_Init(
    &telemetry_h->base,
    name,
    &telemetry_ops_justfloat
  );

  telemetry_h->uart_h = uart_h;
  telemetry_h->channel_count = channel_count;
  telemetry_h->busy = false;
  telemetry_h->sent_frames = 0u;
  telemetry_h->dropped_frames = 0u;
  telemetry_h->error_count = 0u;

  dev_uart_callbacks_t callbacks = {
    .tx_complete_cb = dev_justfloat_uart_ontxcplt,
    .context = telemetry_h,
  };
  Dev_UART_RegisterCallback(uart_h, &callbacks);
}
