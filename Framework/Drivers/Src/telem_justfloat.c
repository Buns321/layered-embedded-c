/**
 * @file telem_justfloat.c
 * @brief JustFloat 二进制浮点遥测协议实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现 IEEE-754 float32 小端序列化、
 *          未置位通道 NaN 占位、`0x00 0x00 0x80 0x7F` 帧尾拼接与 UART DMA 提交。
 * @ingroup Framework_Drivers
 */

#include "telem_justfloat.h"
#include "telem_uart_tx.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/** @brief JustFloat 协议固定 4 字节帧尾（IEEE-754 小端表示的正无穷 +Inf）。 */
static const uint8_t s_justfloat_tail[JUSTFLOAT_FRAME_TAIL_SIZE] = {
    0x00U,
    0x00U,
    0x80U,
    0x7FU,
};

/**
 * @brief 将单精度浮点数以小端字节序写入目标缓冲区（避免非对齐指针强转）。
 * @param[out] dst   至少 4 字节的目标缓冲区指针。
 * @param[in]  value 待序列化的单精度浮点数。
 */
static void justfloat_store_f32_le(uint8_t *dst, float value) {
  uint32_t bits = 0U;
  (void)memcpy(&bits, &value, sizeof(bits));
  dst[0] = (uint8_t)(bits & 0xFFU);
  dst[1] = (uint8_t)((bits >> 8U) & 0xFFU);
  dst[2] = (uint8_t)((bits >> 16U) & 0xFFU);
  dst[3] = (uint8_t)((bits >> 24U) & 0xFFU);
}

/**
 * @brief JustFloat 虚表回调：打包 N 路小端浮点与 4 字节帧尾并提交 DMA 发送。
 * @param[in,out] telemetry_h 遥测基类指针（实际指向 telem_justfloat_t）。
 * @param[in]     sample      待发送遥测样本。
 * @return 提交状态码。
 */
static dev_telemetry_status_t justfloat_send(telemetry_base_t *telemetry_h,
                                             const telemetry_sample_t *sample) {
  telem_justfloat_t *jf = (telem_justfloat_t *)telemetry_h;
  if (jf == NULL || sample == NULL) {
    return DEV_TELEMETRY_ERROR;
  }

  // 若上一次 DMA 仍在发送，直接丢弃本帧避免覆写 tx_buffer
  if (!Telem_UartTx_TryBegin(&jf->uart, &jf->tx)) {
    return DEV_TELEMETRY_BUSY;
  }

  if (jf->channel_count == 0U || jf->channel_count > TELEMETRY_CHANNEL_CAPACITY) {
    Telem_UartTx_RecordError(&jf->tx);
    return DEV_TELEMETRY_ERROR;
  }

  // JustFloat 帧结构：[ch0_f32_le ... ch(N-1)_f32_le][0x00, 0x00, 0x80, 0x7F]
  // 未在 valid_mask 中置位的通道填入 NaN，上位机据此保持该通道断线/无效而不偏移后续通道索引
  for (uint8_t ch = 0U; ch < jf->channel_count; ++ch) {
    uint32_t mask = (uint32_t)1U << ch;
    float value = ((sample->valid_mask & mask) != 0U) ? sample->value[ch] : NAN;
    justfloat_store_f32_le(&jf->tx_buffer[(size_t)ch * 4U], value);
  }

  uint32_t payload_size = (uint32_t)jf->channel_count * 4U;
  (void)memcpy(&jf->tx_buffer[payload_size], s_justfloat_tail, JUSTFLOAT_FRAME_TAIL_SIZE);
  return Telem_UartTx_SubmitDMA(&jf->uart, &jf->tx, jf->tx_buffer,
                                (uint16_t)(payload_size + JUSTFLOAT_FRAME_TAIL_SIZE));
}

/**
 * @brief JustFloat 虚表回调：读取异步发送状态与统计计数。
 * @param[in]  telemetry_h 遥测基类指针（实际指向 telem_justfloat_t）。
 * @param[out] status      状态输出结构体指针。
 */
static void justfloat_get_status(const telemetry_base_t *telemetry_h, telemetry_status_t *status) {
  const telem_justfloat_t *jf = (const telem_justfloat_t *)telemetry_h;
  if (jf == NULL || status == NULL) {
    return;
  }
  Telem_UartTx_GetStatus(&jf->uart, &jf->tx, status);
}

/** @brief JustFloat 遥测多态虚表实例。 */
static const telemetry_ops_t s_telem_ops_justfloat = {
    .send = justfloat_send,
    .get_status = justfloat_get_status,
};

bsp_status_t Dev_Telem_JustFloat_Init(telem_justfloat_t *jf, const telem_justfloat_cfg_t *cfg) {
  if (jf == NULL || cfg == NULL || cfg->name == NULL || cfg->huart == NULL ||
      cfg->channel_count == 0U || cfg->channel_count > TELEMETRY_CHANNEL_CAPACITY) {
    return BSP_ERR_INVALID_ARG;
  }

  bsp_status_t st = Telem_UartTx_Init(&jf->uart, &jf->tx, cfg->huart);
  if (st != BSP_OK) {
    return st;
  }

  jf->channel_count = cfg->channel_count;
  Dev_Telemetry_Base_Init(&jf->base, cfg->name, &s_telem_ops_justfloat);
  return BSP_OK;
}
