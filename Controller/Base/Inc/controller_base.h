//
// Created by Buns_ on 2026/10/17.
//

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_BASE_H
#define LAYERED_EMBEDDED_C_CONTROLLER_BASE_H

#include <stdint.h>

typedef struct controller_base controller_base_t;

typedef struct controller_ops {
  float (*update) (controller_base_t *this, float target, float measure);
  void  (*reset)  (controller_base_t *this);
} controller_ops_t;

typedef struct controller_base {
  const controller_ops_t *ops;
  const char             *name;
} controller_base_t;

void Ctrl_Base_Init(controller_base_t *this, const char *name, const controller_ops_t *ops);
float Ctrl_Base_Update(controller_base_t *this, float target, float measure);
void Ctrl_Base_Reset(controller_base_t *this);

#endif //LAYERED_EMBEDDED_C_CONTROLLER_BASE_H
