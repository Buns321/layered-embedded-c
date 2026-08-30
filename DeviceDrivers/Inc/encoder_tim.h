//
// Created by Buns_ on 2026/8/30.
//

#ifndef LAYERED_EMBEDDED_C_ENCODER_TIM_H
#define LAYERED_EMBEDDED_C_ENCODER_TIM_H

#include "../Base/Inc/encoder_base.h"
#include "../../Platform/Inc/platform_tim.h"
#include <stdbool.h>

typedef struct {
  encoder_base_t encoder_base;
  Platform_TIM_Instance tim;
  Platform_TIM_Channel channel;
  int32_t position;
  int32_t last_count;
  bool is_first;
} encoder_tim_t;

void Dev_Encoder_TIM_Init(encoder_tim_t *this, const char *name, Platform_TIM_Instance tim, Platform_TIM_Channel channel);

#endif //LAYERED_EMBEDDED_C_ENCODER_TIM_H
