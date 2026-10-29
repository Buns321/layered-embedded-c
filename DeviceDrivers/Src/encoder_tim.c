//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_tim.c
 * @brief 基于 TIM 的编码器驱动实现。
 * @ingroup drivers_concrete
 */

#include "encoder_tim.h"
#include "platform_encoder.h"
#include "assert.h"

/**
 * @brief 读取硬件计数并更新累计位置。
 */
static int32_t dev_encoder_tim_readdelta(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  uint32_t now = 0u;
  if (Platform_Encoder_GetCounter(encoder_tim_h->tim, &now) != PLATFORM_STATUS_OK) {
    encoder_tim_h->overflow = true;
    return 0;
  }
  if (encoder_tim_h->has_base_count == false) {
    encoder_tim_h->last_count = now;
    encoder_tim_h->has_base_count = true;
    return 0;
  }

  uint32_t diff = now - encoder_tim_h->last_count;
  int32_t delta;

  if (encoder_tim_h->bit_width == 16U) {
    diff &= 0xFFFFU;
    if (diff >= 0x8000U) {
      delta = (int32_t)((int64_t)diff - 0x10000LL);
    } else {
      delta = (int32_t)diff;
    }
  } else {
    if (diff >= 0x80000000U) {
      delta = (int32_t)((int64_t)diff - 0x100000000LL);
    } else {
      delta = (int32_t)diff;
    }
  }

  if (encoder_tim_h->max_counts_per_period != 0u) {
    int64_t delta64 = (int64_t)delta;
    int64_t limit = (int64_t)encoder_tim_h->max_counts_per_period;

    if (delta64 > limit) {
      delta64 = limit;
      encoder_tim_h->overflow = true;
    } else if (delta64 < -limit) {
      delta64 = -limit;
      encoder_tim_h->overflow = true;
    }

    delta = (int32_t)delta64;
  }

  encoder_tim_h->last_count = now;
  encoder_tim_h->position += delta;
  return delta;
}

/**
 * @brief 返回累计编码器位置。
 */
static int64_t dev_encoder_tim_getposition(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  return encoder_tim_h->position;
}

/**
 * @brief 清零累计编码器位置。
 */
static void dev_encoder_tim_setzero(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  if (Platform_Encoder_SetCounter(encoder_tim_h->tim, 0u) != PLATFORM_STATUS_OK) {
    encoder_tim_h->overflow = true;
    return;
  }
  encoder_tim_h->last_count = 0;
  encoder_tim_h->has_base_count = true;
  encoder_tim_h->overflow = false;
  encoder_tim_h->position = 0;
}

/**
 * @brief 返回最近一次读取的计数增量。
 */
static int32_t dev_encoder_tim_getdelta(encoder_base_t *encoder_h) {
  assert(encoder_h);
  return dev_encoder_tim_readdelta(encoder_h);
}

/**
 * @brief 在检测到方向变化后重新同步计数基准。
 */
static void dev_encoder_tim_resync(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  uint32_t counter = 0u;
  if (Platform_Encoder_GetCounter(encoder_tim_h->tim, &counter) != PLATFORM_STATUS_OK) {
    encoder_tim_h->overflow = true;
    return;
  }
  encoder_tim_h->last_count = counter;
  encoder_tim_h->has_base_count = true;
  encoder_tim_h->overflow = false;
}

static const encoder_ops_t encoder_ops_tim = {
  .get_position = dev_encoder_tim_getposition,
  .set_zero = dev_encoder_tim_setzero,
  .get_delta = dev_encoder_tim_getdelta,
  .resync = dev_encoder_tim_resync,
};

/**
 * @brief 初始化并启动定时器编码器。
 * @param encoder_h 对象。
 * @param name 名称。
 * @param tim 定时器身份。
 * @retval PLATFORM_STATUS_OK 启动成功。
 * @retval PLATFORM_STATUS_ERROR 启动失败。
 */
platform_status_t Dev_Encoder_TIM_Init(
  encoder_tim_t *encoder_h,
  const char *name,
  platform_encoder_instance_t tim
) {
  assert(encoder_h && name);

  encoder_h->tim = tim;
  platform_status_t status = Platform_Encoder_GetCounterBits(
    encoder_h->tim,
    &encoder_h->bit_width
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  assert(encoder_h->bit_width == 16U || encoder_h->bit_width == 32U);

  Dev_Encoder_Base_Init(&encoder_h->encoder_base, name, &encoder_ops_tim);

  encoder_h->position = 0;
  encoder_h->last_count = 0;
  encoder_h->has_base_count = false;
  encoder_h->overflow = false;
  encoder_h->max_counts_per_period = 0u;

  return Platform_Encoder_Start(encoder_h->tim);
}

/**
 * @brief 设置单周期增量上限。
 * @param encoder_h 编码器对象。
 * @param max_counts 上限（counts/period）；0 表示不限制。
 * @note 超限时增量会被夹紧并置 overflow 标志。
 */
void Dev_Encoder_TIM_SetMaxCountsPerPeriod(encoder_tim_t *encoder_h, uint32_t max_counts) {
  assert(encoder_h);
  encoder_h->max_counts_per_period = max_counts;
}

/** @brief 是否发生过单周期超限。 */
bool Dev_Encoder_TIM_HasOverflow(const encoder_tim_t *encoder_h) {
  assert(encoder_h);
  return encoder_h->overflow;
}

/** @brief 清除 overflow 标志。 */
void Dev_Encoder_TIM_ClearOverflow(encoder_tim_t *encoder_h) {
  assert(encoder_h);
  encoder_h->overflow = false;
}
