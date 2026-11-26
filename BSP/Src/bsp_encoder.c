/**
 * @file bsp_encoder.c
 * @brief 定时器正交编码器解码与位置累计实现。
 * @ingroup bsp
 */

#include "bsp_encoder.h"
#include "bsp_hal_internal.h"
#include <limits.h>
#include <string.h>

static uint32_t bsp_encoder_readraw(const bsp_encoder_t *enc) {
  uint32_t raw = __HAL_TIM_GET_COUNTER(enc->htim);
  if (enc->bit_width == 16U) {
    raw &= 0xFFFFU;
  }
  return raw;
}

bsp_status_t BSP_Encoder_Init(bsp_encoder_t *enc, const bsp_encoder_cfg_t *cfg) {
  if (enc == NULL || cfg == NULL || cfg->htim == NULL || cfg->htim->Instance == NULL) {
    return BSP_ERR_INVALID_ARG;
  }

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
  if (!enc->has_base) {
    enc->last_count = now;
    enc->has_base = true;
    return 0;
  }

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
