//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_tim.h
 * @brief 基于定时器编码器模式的编码器实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_ENCODER_TIM_H
#define LAYERED_EMBEDDED_C_ENCODER_TIM_H

#include "encoder_base.h"
#include "platform_encoder.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 定时器编码器对象。
 * @note position 使用 int64 累计，降低长期溢出风险。
 * @note max_counts_per_period 为 0 时不限制单周期增量。
 */
typedef struct encoder_tim_s {
  encoder_base_t encoder_base;
  platform_encoder_instance_t tim;
  uint8_t bit_width;

  bool has_base_count;
  bool overflow;

  uint32_t max_counts_per_period; // 0 = 不限制

  int64_t position;
  uint32_t last_count;
} encoder_tim_t;

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
);
/**
 * @brief 设置单周期增量上限。
 * @param encoder_h 编码器对象。
 * @param max_counts 上限（counts/period）；0 表示不限制。
 * @note 超限时增量会被夹紧并置 overflow 标志。
 */
void Dev_Encoder_TIM_SetMaxCountsPerPeriod(encoder_tim_t *encoder_h, uint32_t max_counts);
/** @brief 是否发生过单周期超限。 */
bool Dev_Encoder_TIM_HasOverflow(const encoder_tim_t *encoder_h);
/** @brief 清除 overflow 标志。 */
void Dev_Encoder_TIM_ClearOverflow(encoder_tim_t *encoder_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_ENCODER_TIM_H
