//
// Created by Buns_ on 2026/8/30.
//

#ifndef LAYERED_EMBEDDED_C_ENCODER_BASE_H
#define LAYERED_EMBEDDED_C_ENCODER_BASE_H

#include <stdint.h>

typedef struct encoder_base encoder_base_t;

typedef struct encoder_ops {
  void    (*set_zero)    (encoder_base_t *this);
  int32_t (*get_position)(encoder_base_t *this);
} encoder_ops_t;

typedef struct encoder_base {
  const encoder_ops_t *ops;
  const char *name;
} encoder_base_t;

void Dev_Encoder_Base_Init(encoder_base_t *this, const char *name, const encoder_ops_t *ops);
void Dev_Encoder_Base_SetZero(encoder_base_t *this);
int32_t Dev_Encoder_Base_GetPosition(encoder_base_t *this);

#endif //LAYERED_EMBEDDED_C_ENCODER_BASE_H
