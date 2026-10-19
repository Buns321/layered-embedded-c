//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_FILTER_LOWPASS_H
#define LAYERED_EMBEDDED_C_FILTER_LOWPASS_H

#include "filter_base.h"
#include <stdint.h>

typedef struct filter_lowpass {
  filter_base_t filter_base;

  float    last_output     [FILTER_CHANNEL_CAPACITY];
  uint32_t last_time_ms    [FILTER_CHANNEL_CAPACITY];
  float    time_constant_s [FILTER_CHANNEL_CAPACITY];
  uint32_t initialized_mask;
} filter_lowpass_t;

void Filter_Lowpass_Init(filter_lowpass_t *this, const char *name, float default_time_constant_s);
void Filter_Lowpass_SetChannelTimeConstant(filter_lowpass_t *this, uint8_t channel, float time_constant_s);

#endif //LAYERED_EMBEDDED_C_FILTER_LOWPASS_H
