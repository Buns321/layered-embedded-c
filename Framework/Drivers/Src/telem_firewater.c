/**
 * @file telem_firewater.c
 * @brief FireWater 文本遥测协议实现。
 * @ingroup drivers
 */

#include "telem_firewater.h"
#include "sys_math.h"
#include "telem_uart_tx.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define FIREWATER_MAX_PRECISION 6U
#define FIREWATER_MAX_SCALED_VALUE 1000000000.0f

static const uint32_t s_pow10[] = {
    1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U,
};

static bool firewater_append_char(uint8_t *buf, uint32_t cap, uint32_t *len, char ch) {
  if (*len >= cap) {
    return false;
  }
  buf[*len] = (uint8_t)ch;
  (*len)++;
  return true;
}

static bool firewater_append_str(uint8_t *buf, uint32_t cap, uint32_t *len, const char *str) {
  uint32_t str_len = (uint32_t)strlen(str);
  if (*len > cap || str_len > (cap - *len)) {
    return false;
  }
  for (uint32_t i = 0U; i < str_len; ++i) {
    buf[*len + i] = (uint8_t)str[i];
  }
  *len += str_len;
  return true;
}

static bool firewater_append_u32(uint8_t *buf, uint32_t cap, uint32_t *len, uint32_t value) {
  char digits[10];
  uint32_t count = 0U;
  do {
    digits[count++] = (char)('0' + (value % 10U));
    value /= 10U;
  } while (value != 0U);

  if (*len > cap || count > (cap - *len)) {
    return false;
  }
  while (count > 0U) {
    buf[(*len)++] = (uint8_t)digits[--count];
  }
  return true;
}

static bool firewater_append_f32(uint8_t *buf, uint32_t cap, uint32_t *len, float value,
                                 uint8_t precision) {
  if (!SYS_ISFINITE(value) || precision > FIREWATER_MAX_PRECISION) {
    return false;
  }

  uint32_t scale = s_pow10[precision];
  float scaled = value * (float)scale;
  if (!SYS_ISFINITE(scaled) || SYS_ABS(scaled) > FIREWATER_MAX_SCALED_VALUE) {
    return false;
  }

  int32_t rounded = (scaled >= 0.0f) ? (int32_t)(scaled + 0.5f) : (int32_t)(scaled - 0.5f);
  uint32_t mag = (rounded < 0) ? (uint32_t)(-rounded) : (uint32_t)rounded;

  if (rounded < 0) {
    if (!firewater_append_char(buf, cap, len, '-')) {
      return false;
    }
  }

  if (!firewater_append_u32(buf, cap, len, mag / scale)) {
    return false;
  }
  if (precision == 0U) {
    return true;
  }
  if (!firewater_append_char(buf, cap, len, '.')) {
    return false;
  }

  uint32_t dec = mag % scale;
  for (uint8_t idx = precision; idx > 0U; --idx) {
    uint32_t divisor = s_pow10[idx - 1U];
    char digit = (char)('0' + ((dec / divisor) % 10U));
    if (!firewater_append_char(buf, cap, len, digit)) {
      return false;
    }
  }
  return true;
}

static bool firewater_encode(telem_firewater_t *fw, const telemetry_sample_t *sample,
                             uint32_t *out_len) {
  uint32_t len = 0U;
  bool has_value = false;

  for (uint8_t ch = 0U; ch < fw->channel_count; ++ch) {
    uint32_t mask = (uint32_t)1U << ch;
    if ((sample->valid_mask & mask) == 0U) {
      continue;
    }
    if (has_value && !firewater_append_char(fw->tx_buffer, FIREWATER_UART_TX_CAPACITY, &len, ',')) {
      return false;
    }
    if (!firewater_append_str(fw->tx_buffer, FIREWATER_UART_TX_CAPACITY, &len,
                              fw->channel_names[ch]) ||
        !firewater_append_char(fw->tx_buffer, FIREWATER_UART_TX_CAPACITY, &len, ':') ||
        !firewater_append_f32(fw->tx_buffer, FIREWATER_UART_TX_CAPACITY, &len, sample->value[ch],
                              fw->precision)) {
      return false;
    }
    has_value = true;
  }

  if (!has_value || !firewater_append_char(fw->tx_buffer, FIREWATER_UART_TX_CAPACITY, &len, '\n')) {
    return false;
  }

  *out_len = len;
  return true;
}

static dev_telemetry_status_t firewater_send(telemetry_base_t *telemetry_h,
                                             const telemetry_sample_t *sample) {
  telem_firewater_t *fw = (telem_firewater_t *)telemetry_h;
  if (fw == NULL || sample == NULL) {
    return DEV_TELEMETRY_ERROR;
  }

  if (!Telem_UartTx_TryBegin(&fw->uart, &fw->tx)) {
    return DEV_TELEMETRY_BUSY;
  }

  uint32_t frame_size = 0U;
  if (!firewater_encode(fw, sample, &frame_size)) {
    Telem_UartTx_RecordError(&fw->tx);
    return DEV_TELEMETRY_ERROR;
  }

  return Telem_UartTx_SubmitDMA(&fw->uart, &fw->tx, fw->tx_buffer, (uint16_t)frame_size);
}

static void firewater_get_status(const telemetry_base_t *telemetry_h, telemetry_status_t *status) {
  const telem_firewater_t *fw = (const telem_firewater_t *)telemetry_h;
  if (fw == NULL || status == NULL) {
    return;
  }
  Telem_UartTx_GetStatus(&fw->uart, &fw->tx, status);
}

static const telemetry_ops_t s_telem_ops_firewater = {
    .send = firewater_send,
    .get_status = firewater_get_status,
};

bsp_status_t Dev_Telem_FireWater_Init(telem_firewater_t *fw, const telem_firewater_cfg_t *cfg) {
  if (fw == NULL || cfg == NULL || cfg->name == NULL || cfg->huart == NULL ||
      cfg->channel_names == NULL || cfg->channel_count == 0U ||
      cfg->channel_count > TELEMETRY_CHANNEL_CAPACITY || cfg->precision > FIREWATER_MAX_PRECISION) {
    return BSP_ERR_INVALID_ARG;
  }

  for (uint8_t ch = 0U; ch < cfg->channel_count; ++ch) {
    if (cfg->channel_names[ch] == NULL || cfg->channel_names[ch][0] == '\0' ||
        strchr(cfg->channel_names[ch], ',') != NULL ||
        strchr(cfg->channel_names[ch], ':') != NULL) {
      return BSP_ERR_INVALID_ARG;
    }
  }

  bsp_status_t st = Telem_UartTx_Init(&fw->uart, &fw->tx, cfg->huart);
  if (st != BSP_OK) {
    return st;
  }

  fw->channel_names = cfg->channel_names;
  fw->channel_count = cfg->channel_count;
  fw->precision = cfg->precision;
  Dev_Telemetry_Base_Init(&fw->base, cfg->name, &s_telem_ops_firewater);
  return BSP_OK;
}
