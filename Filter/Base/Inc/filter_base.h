//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_FILTER_BASE_H
#define LAYERED_EMBEDDED_C_FILTER_BASE_H

#include "filter_sample.h"

typedef struct filter_base filter_base_t;

typedef struct filter_ops {
  void (*update)(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output);
  void (*reset)(filter_base_t *this);
} filter_ops_t;

typedef struct filter_base {
  const filter_ops_t *ops;
  const char         *name;
} filter_base_t;

void Filter_Base_Init(filter_base_t *this, const char *name, const filter_ops_t *ops);
void Filter_Base_Update(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output);
void Filter_Base_Reset(filter_base_t *this);

#endif //LAYERED_EMBEDDED_C_FILTER_BASE_H
