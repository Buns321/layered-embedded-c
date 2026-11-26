/**
 * @file telem_justfloat.c
 * @brief JustFloat 二进制浮点遥测协议实现。
 * @ingroup drivers
 */

#include "telem_justfloat.h"
#include "telem_uart_tx.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static const uint8_t s_justfloat_tail[JUSTFLOAT_FRAME_TAIL_SIZE] = {
    0x00U,
    0x00U,
    0x80U,
    0x7FU,
};

static void justfloat_store_f32_le(uint8_t *dst, float value) {
  uint32_t bits = 0U;
  (void)memcpy(&bits, &value, sizeof(bits));
  dst[0] = (uint8_t)(bits & 0xFFU);
  dst[1] = (uint8_t)((bits >> 8U) & 0xFFU);
  dst[2] = (uint8_t)((bits >> 16U) & 0xFFU);
  dst[3] = (uint8_t)((bits >> 24U) & 0xFFU);
}

static dev_telemetry_status_t justfloat_send(telemetry_base_t *telemetry_h,
                                             const telemetry_sample_t *sample) {
  telem_justfloat_t *jf = (telem_justfloat_t *)telemetry_h;
  if (jf == NULL || sample == NULL) {
    return DEV_TELEMETRY_ERROR;
  }

  if (!Telem_UartTx_TryBegin(&jf->uart, &jf->tx)) {
    return DEV_TELEMETRY_BUSY;
  }

  if (jf->channel_count == 0U || jf->channel_count > TELEMETRY_CHANNEL_CAPACITY) {
    Telem_UartTx_RecordError(&jf->tx);
    return DEV_TELEMETRY_ERROR;
  }

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

static void justfloat_get_status(const telemetry_base_t *telemetry_h, telemetry_status_t *status) {
  const telem_justfloat_t *jf = (const telem_justfloat_t *)telemetry_h;
  if (jf == NULL || status == NULL) {
    return;
  }
  Telem_UartTx_GetStatus(&jf->uart, &jf->tx, status);
}

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
