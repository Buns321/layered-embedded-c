/**
 * @file bsp_encoder.c
 * @brief 定时器正交编码器解码与位置累计实现。
 * @details 在初始化阶段自动识别 16/32 位硬件计数器位宽并启动编码器接口，
 *          在周期采样时通过有符号半量程展开消除计数器回绕跳变，并支持极性反转与单周期限幅。
 * @ingroup BSP
 */

#include "bsp_encoder.h"
#include "bsp_hal_internal.h"
#include <limits.h>
#include <string.h>

/**
 * @brief 读取编码器硬件定时器当前原始计数值。
 * @param[in] enc 编码器运行期对象指针（须保证 `enc` 与 `enc->htim` 非空）。
 * @return 按 `bit_width` 掩码截断后的无符号计数值（16 位或 32 位）。
 */
static uint32_t bsp_encoder_readraw(const bsp_encoder_t *enc) {
  uint32_t raw = __HAL_TIM_GET_COUNTER(enc->htim);
  if (enc->bit_width == 16U) {
    // 16 位定时器仅保留低 16 位，防止宿主机桩或高位残留干扰回绕计算
    raw &= 0xFFFFU;
  }
  return raw;
}

bsp_status_t BSP_Encoder_Init(bsp_encoder_t *enc, const bsp_encoder_cfg_t *cfg) {
  if (enc == NULL || cfg == NULL || cfg->htim == NULL || cfg->htim->Instance == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

  // 必须使用满量程 Period（0xFFFF 或 32 位定时器的 0xFFFFFFFF），否则半量程回绕展开公式不成立
  uint8_t bit_width = 0U;
  if (cfg->htim->Init.Period == 0xFFFFFFFFU && IS_TIM_32B_COUNTER_INSTANCE(cfg->htim->Instance)) {
    bit_width = 32U;
  } else if (cfg->htim->Init.Period == 0xFFFFU) {
    bit_width = 16U;
  } else {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(enc, 0, sizeof(*enc));
  enc->htim = cfg->htim;
  enc->max_counts_per_period = cfg->max_counts_per_period;
  enc->bit_width = bit_width;
  enc->invert = cfg->invert;

  // 正交编码器需同时使能 CH1/CH2 输入捕获通道（TIM_CHANNEL_ALL）
  HAL_StatusTypeDef hal_st = HAL_TIM_Encoder_Start(enc->htim, TIM_CHANNEL_ALL);
  if (hal_st != HAL_OK) {
    enc->htim = NULL;
    return bsp_status_from_hal(hal_st);
  }
  return BSP_OK;
}

int32_t BSP_Encoder_GetDelta(bsp_encoder_t *enc) {
  if (enc == NULL || enc->htim == NULL) {
    return 0;
  }

  uint32_t now = bsp_encoder_readraw(enc);
  // 首帧仅建立采样基线，避免上电前机械抖动或初始 CNT 偏移被误算为首周期速度尖峰
  if (!enc->has_base) {
    enc->last_count = now;
    enc->has_base = true;
    return 0;
  }

  // 通过无符号模运算差值 + 半量程有符号展开，将 16/32 位回绕还原为 64 位带符号增量
  int64_t delta64 = 0;
  if (enc->bit_width == 16U) {
    uint32_t diff = (now - enc->last_count) & 0xFFFFU;
    delta64 = (diff >= 0x8000U) ? ((int64_t)diff - 0x10000LL) : (int64_t)diff;
  } else {
    uint32_t diff = now - enc->last_count;
    delta64 = (diff >= 0x80000000U) ? ((int64_t)diff - 0x100000000LL) : (int64_t)diff;
  }

  if (enc->invert) {
    delta64 = -delta64;
  }

  // 若配置了单周期限幅阈值则对称夹紧并锁存 overflow；未限幅时仅防止极端 32 位增量超出 int32_t
  // 正上限
  if (enc->max_counts_per_period != 0U) {
    int64_t limit = (enc->max_counts_per_period > (uint32_t)INT32_MAX)
                        ? (int64_t)INT32_MAX
                        : (int64_t)enc->max_counts_per_period;
    if (delta64 > limit) {
      delta64 = limit;
      enc->overflow = true;
    } else if (delta64 < -limit) {
      delta64 = -limit;
      enc->overflow = true;
    }
  } else if (delta64 > (int64_t)INT32_MAX) {
    delta64 = (int64_t)INT32_MAX;
    enc->overflow = true;
  }

  enc->last_count = now;
  enc->position_counts += delta64;
  return (int32_t)delta64;
}

int64_t BSP_Encoder_GetPosition(const bsp_encoder_t *enc) {
  if (enc == NULL) {
    return 0;
  }
  return enc->position_counts;
}

void BSP_Encoder_Resync(bsp_encoder_t *enc) {
  if (enc == NULL || enc->htim == NULL) {
    return;
  }
  enc->last_count = bsp_encoder_readraw(enc);
  enc->has_base = true;
  enc->overflow = false;
}

void BSP_Encoder_ResetZero(bsp_encoder_t *enc) {
  if (enc == NULL || enc->htim == NULL) {
    return;
  }
  __HAL_TIM_SET_COUNTER(enc->htim, 0U);
  enc->last_count = 0U;
  enc->position_counts = 0;
  enc->has_base = true;
  enc->overflow = false;
}

bool BSP_Encoder_HasOverflow(const bsp_encoder_t *enc) {
  if (enc == NULL) {
    return false;
  }
  return enc->overflow;
}

void BSP_Encoder_ClearOverflow(bsp_encoder_t *enc) {
  if (enc == NULL) {
    return;
  }
  enc->overflow = false;
}
