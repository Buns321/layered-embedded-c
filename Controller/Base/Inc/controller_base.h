//
// Created by Buns_ on 2026/10/17.
//

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_BASE_H
#define LAYERED_EMBEDDED_C_CONTROLLER_BASE_H

#include <stdint.h>

typedef struct controller_base controller_base_t;

typedef enum {
  CONTROLLER_STATUS_OK = 0U,
  CONTROLLER_STATUS_SATURATED,
  CONTROLLER_STATUS_ERROR,
} controller_status_t;

/*
 * 契约：
 * - target / measure：当前槽位约定的物理量（轮速环为 m/s）
 * - dt：控制周期，单位 s，必须大于 0
 * - out：执行器量（轮速环为 duty），范围由控制器与电机上限共同约束
 * - 返回 CONTROLLER_STATUS_OK / SATURATED / ERROR
 */
typedef struct controller_ops {
  controller_status_t (*update)(
    controller_base_t *this,
    float target,
    float measure,
    float dt,
    float *out
  );
  void (*reset)(controller_base_t *this);
} controller_ops_t;

typedef struct controller_base {
  const controller_ops_t *ops;
  const char             *name;
} controller_base_t;

void Ctrl_Base_Init(controller_base_t *this, const char *name, const controller_ops_t *ops);
controller_status_t Ctrl_Base_Update(
  controller_base_t *this,
  float target,
  float measure,
  float dt,
  float *out
);
void Ctrl_Base_Reset(controller_base_t *this);

#endif //LAYERED_EMBEDDED_C_CONTROLLER_BASE_H
