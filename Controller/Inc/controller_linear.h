//
// Created by Buns_ on 2026/10/17.
//

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_LINEAR_H
#define LAYERED_EMBEDDED_C_CONTROLLER_LINEAR_H

#include "controller_base.h"
#include <stdbool.h>

typedef struct controller_linear {
  controller_base_t controller_base;

  float k;                     // 斜率
  float b;                     // 截距

  bool  is_defriction_enabled; // 是否启用摩擦补偿

  float max_target;            // 最大输入限幅 (m/s)
  float min_target;            // 最小输入限幅 (m/s)
  float max_out;               // 最大输出限幅 (duty)
  float min_out;               // 最小输出限幅 (duty)
  float max_step;              // 最大步长限幅 (duty)
  float min_step;              // 最小步长限幅 (duty)
  float deadzone;              // 死区         (duty)

  // TODO 突然想到貌似应该要做一个文件里面存类似于 HAL_MAX_DELAY 的宏定义 by Buns_ at 2026/10/17 16:40

  float last_out;              // 上次输出寄存器，用于单次控制限幅 (duty)
} controller_linear_t;

void Ctrl_Linear_Init(
  controller_linear_t *controller_linear,
  const char *name,
  float k,                     // 斜率
  float b,                     // 截距
  bool  is_defriction_enabled, // 是否启用摩擦补偿
  float max_target,            // 最大输入限幅 (m/s)
  float min_target,            // 最小输入限幅 (m/s)
  float max_out,               // 最大输出限幅 (duty)
  float min_out,               // 最小输出限幅 (duty)
  float max_step,              // 最大步长限幅 (duty)
  float min_step,              // 最小步长限幅 (duty)
  float deadzone               // 死区         (duty)
);

#endif //LAYERED_EMBEDDED_C_CONTROLLER_LINEAR_H
