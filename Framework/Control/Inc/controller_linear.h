/**
 * @file controller_linear.h
 * @brief 线性前馈/开环控制律：线性映射、摩擦符号补偿、步长限幅与保持带死区。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_CONTROLLER_LINEAR_H
#define LAYERED_CAR_CONTROLLER_LINEAR_H

#include "controller_base.h"
#include <stdbool.h>

/**
 * @brief 线性控制律配置参数（target 单位 m/s，输出与步长单位 duty）。
 */
typedef struct ctrl_linear_cfg_s {
  float k;
  float b;
  bool defriction_enabled;
  float max_target; /* 0 且 min_target==0 表示不限制输入 */
  float min_target; /* min_target==0 && max_target>0 时默认取 -max_target */
  float max_out;    /* 必填 > 0 */
  float min_out;    /* 0 时默认取 -max_out */
  float max_step;   /* 0 表示不限制单步变化量（此时 min_step 须为 0） */
  float min_step;
  float deadzone;
} ctrl_linear_cfg_t;

/**
 * @brief 线性控制律静态实例对象（base 必须为首成员）。
 */
typedef struct controller_linear_s {
  controller_base_t base;
  float k;
  float b;
  bool defriction_enabled;
  float max_target;
  float min_target;
  float max_out;
  float min_out;
  float max_step;
  float min_step;
  float deadzone;
  bool limit_target;
  bool limit_step;
  float last_out;
} controller_linear_t;

/**
 * @brief 校验配置并初始化线性控制律；失败时返回 false 且不修改 linear。
 */
bool Ctrl_Linear_Init(controller_linear_t *linear, const char *name, const ctrl_linear_cfg_t *cfg);

/** @} */

#endif /* LAYERED_CAR_CONTROLLER_LINEAR_H */
