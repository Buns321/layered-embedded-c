//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_FILTER_SAMPLE_H
#define LAYERED_EMBEDDED_C_FILTER_SAMPLE_H

#include <stdint.h>

#define FILTER_CHANNEL_CAPACITY 16u
_Static_assert(FILTER_CHANNEL_CAPACITY > 0u, "FILTER_CHANNEL_CAPACITY must be positive");
_Static_assert(FILTER_CHANNEL_CAPACITY <= 32u, "FILTER_CHANNEL_CAPACITY must not exceed 32");
#define FILTER_CHANNEL_MASK \
  ((uint32_t)((1u << FILTER_CHANNEL_CAPACITY) - 1u))

typedef struct {
  float    value[FILTER_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} filter_sample_t;

#endif //LAYERED_EMBEDDED_C_FILTER_SAMPLE_H
