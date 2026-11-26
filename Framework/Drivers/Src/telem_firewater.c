/**
 * @file telem_firewater.c
 * @brief FireWater 文本遥测协议实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现无 snprintf 浮点格式化、
 *          `ch0:v0,ch1:v1\n` 帧编码与 UART DMA 非阻塞提交。
 * @ingroup Framework_Drivers
 */

#include "telem_firewater.h"
#include "sys_math.h"
#include "telem_uart_tx.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/** @brief FireWater 支持的最大小数位数。 */
#define FIREWATER_MAX_PRECISION 6U
/** @brief 定点缩放后允许的最大绝对值（防止 int32_t 截断溢出）。 */
#define FIREWATER_MAX_SCALED_VALUE 1000000000.0f

/** @brief 10 的幂次查找表（索引 0..6 对应 1..10^6）。 */
static const uint32_t s_pow10[] = {
    1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U,
};

/**
 * @brief 向发送缓冲区追加单个 ASCII 字符。
 * @param[out]    buf 发送缓冲区首地址。
 * @param[in]     cap 缓冲区总容量（单位：bytes）。
 * @param[in,out] len 当前已写入字节数。
 * @param[in]     ch  待写入字符。
 * @return 缓冲区空间是否足够。
 */
static bool firewater_append_char(uint8_t *buf, uint32_t cap, uint32_t *len, char ch) {
  if (*len >= cap) {
    return false;
  }
  buf[*len] = (uint8_t)ch;
  (*len)++;
  return true;
}

/**
 * @brief 向发送缓冲区追加以空字符结尾的字符串（不含 `\0`）。
 * @param[out]    buf 发送缓冲区首地址。
 * @param[in]     cap 缓冲区总容量（单位：bytes）。
 * @param[in,out] len 当前已写入字节数。
 * @param[in]     str 待写入字符串指针。
 * @return 缓冲区空间是否足够。
 */
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

/**
 * @brief 将无符号 32 位整数格式化为十进制 ASCII 并追加至缓冲区。
 * @param[out]    buf   发送缓冲区首地址。
 * @param[in]     cap   缓冲区总容量（单位：bytes）。
 * @param[in,out] len   当前已写入字节数。
 * @param[in]     value 待格式化整数。
 * @return 缓冲区空间是否足够。
 */
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

/**
 * @brief 无 snprintf 将浮点数按指定小数位数格式化为 ASCII 文本。
 * @param[out]    buf       发送缓冲区首地址。
 * @param[in]     cap       缓冲区总容量（单位：bytes）。
 * @param[in,out] len       当前已写入字节数。
 * @param[in]     value     待格式化有限浮点数。
 * @param[in]     precision 小数位数（0..6）。
 * @return 格式化与缓冲区写入是否成功。
 */
static bool firewater_append_f32(uint8_t *buf, uint32_t cap, uint32_t *len, float value,
                                 uint8_t precision) {
  if (!SYS_ISFINITE(value) || precision > FIREWATER_MAX_PRECISION) {
    return false;
  }

  // 先按 10^precision 缩放并四舍五入为有符号定点整数，避免调用重型 printf 浮点引擎
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

/**
 * @brief 将多通道样本编码为 FireWater 文本帧：`name0:val0,name1:val1\n`。
 * @param[in,out] fw      FireWater 设备对象指针。
 * @param[in]     sample  遥测样本指针。
 * @param[out]    out_len 编码后的帧字节长度（单位：bytes）。
 * @return 编码是否成功（至少包含一个有效通道且未超出缓冲区容量）。
 */
static bool firewater_encode(telem_firewater_t *fw, const telemetry_sample_t *sample,
                             uint32_t *out_len) {
  uint32_t len = 0U;
  bool has_value = false;

  // FireWater 协议帧格式：<ch_name>:<float_val>[,<ch_name>:<float_val>...]\n
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

/**
 * @brief FireWater 虚表回调：检查 DMA 空闲后编码并提交异步发送。
 * @param[in,out] telemetry_h 遥测基类指针（实际指向 telem_firewater_t）。
 * @param[in]     sample      待发送遥测样本。
 * @return 提交状态码。
 */
static dev_telemetry_status_t firewater_send(telemetry_base_t *telemetry_h,
                                             const telemetry_sample_t *sample) {
  telem_firewater_t *fw = (telem_firewater_t *)telemetry_h;
  if (fw == NULL || sample == NULL) {
    return DEV_TELEMETRY_ERROR;
  }

  // 若上一次 DMA 仍在发送，直接丢弃本帧避免覆写 tx_buffer
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

/**
 * @brief FireWater 虚表回调：读取异步发送状态与统计计数。
 * @param[in]  telemetry_h 遥测基类指针（实际指向 telem_firewater_t）。
 * @param[out] status      状态输出结构体指针。
 */
static void firewater_get_status(const telemetry_base_t *telemetry_h, telemetry_status_t *status) {
  const telem_firewater_t *fw = (const telem_firewater_t *)telemetry_h;
  if (fw == NULL || status == NULL) {
    return;
  }
  Telem_UartTx_GetStatus(&fw->uart, &fw->tx, status);
}

/** @brief FireWater 遥测多态虚表实例。 */
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

  // 禁止通道名称包含 FireWater 协议保留分隔符 `,` 与 `:`
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
