//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H
#define LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H

#include "filter_base.h"

typedef struct filter_passthrough {
  filter_base_t filter_base;
} filter_passthrough_t;

void Filter_Passthrough_Init(
  filter_passthrough_t *this,
  const char *name
);

#endif //LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H
