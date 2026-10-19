//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_FILTER_CHAIN_H
#define LAYERED_EMBEDDED_C_FILTER_CHAIN_H

#include "filter_base.h"
#include <stdint.h>

#define FILTER_CHAIN_CAPACITY 8u

typedef struct filter_chain {
  filter_base_t  filter_base;
  filter_base_t *stages[FILTER_CHAIN_CAPACITY];
  uint8_t        stage_count;
} filter_chain_t;

void Filter_Chain_Init(filter_chain_t *this, const char *name, filter_base_t *const *stages, uint8_t stage_count);

#endif //LAYERED_EMBEDDED_C_FILTER_CHAIN_H
