//
// Created by Buns_ on 2026/10/20.
//

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H
#define LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H

#include "controller_base.h"
#include <stdint.h>

#define CONTROLLER_COMPOSE_CAPACITY 8u

typedef struct controller_compose {
  controller_base_t  base;
  controller_base_t *stages[CONTROLLER_COMPOSE_CAPACITY];
  uint8_t            stage_count;
} controller_compose_t;

void Ctrl_Compose_Init(
  controller_compose_t      *this,
  const char                *name,
  controller_base_t *const  *stages,
  uint8_t                    stage_count
);

#endif //LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H
