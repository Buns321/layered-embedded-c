//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H
#define LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H

#include "filter_base.h"
#include <stdint.h>

#define FILTER_DISTRIBUTOR_CAPACITY 8u
#define FILTER_ROUTE_CAPACITY       4u

typedef struct {
  filter_base_t *filter;
  uint8_t input_index[FILTER_ROUTE_CAPACITY];
  uint8_t output_index[FILTER_ROUTE_CAPACITY];
  uint8_t input_count;
  uint8_t output_count;
} filter_distributor_route_t;

typedef struct filter_distributor {
  filter_base_t filter_base;
  filter_distributor_route_t routes[FILTER_DISTRIBUTOR_CAPACITY];
  uint8_t route_count;
} filter_distributor_t;

void Filter_Distributor_Init(filter_distributor_t *this, const char *name, const filter_distributor_route_t *routes, uint8_t route_count);

#endif //LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H
