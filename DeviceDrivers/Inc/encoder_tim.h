//
// Created by Buns_ on 2026/8/30.
//

#ifndef LAYERED_EMBEDDED_C_ENCODER_TIM_H
#define LAYERED_EMBEDDED_C_ENCODER_TIM_H

#include "encoder_base.h"
#include "platform_def.h"
#include "platform_tim.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  encoder_base_t encoder_base;
  Platform_TIM_Instance tim;
  Platform_TIM_Channel channel;
  uint8_t bit_width;

  bool has_base_count;
  bool overflow;

  uint32_t max_counts_per_period; // 0 = 不限制

  int64_t position;
  uint32_t last_count;
} encoder_tim_t;

Platform_StatusTypeDef Dev_Encoder_TIM_Init(encoder_tim_t *this, const char *name, Platform_TIM_Instance tim, Platform_TIM_Channel channel);
void Dev_Encoder_TIM_SetMaxCountsPerPeriod(encoder_tim_t *this, uint32_t max_counts);
bool Dev_Encoder_TIM_HasOverflow(const encoder_tim_t *this);
void Dev_Encoder_TIM_ClearOverflow(encoder_tim_t *this);

#endif //LAYERED_EMBEDDED_C_ENCODER_TIM_H
