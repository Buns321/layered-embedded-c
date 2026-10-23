//
// 由 Buns_ 创建于 2026/10/17。
//

/**
 * @file controller_linear.h
 * @brief 线性控制律：开环映射、限幅、步长、死区与摩擦补偿。
 * @addtogroup controller
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_LINEAR_H
#define LAYERED_EMBEDDED_C_CONTROLLER_LINEAR_H

#include "controller_base.h"
#include <stdbool.h>

/**
 * @brief 线性控制律对象。
 * @note 该控制律是开环映射，忽略 measure 和 dt。
 * @note deadzone 是围绕上次输出的保持带，不是零附近的死区。
 */
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

  // TODO 由 Buns_ 于 2026/10/17 16:40 记录：应增加一个文件，集中存放
  // 类似 HAL_MAX_DELAY 的通用宏定义。

  float last_out;              // 上次输出寄存器，用于单次控制限幅 (duty)
} controller_linear_t;

/**
 * @brief 初始化线性控制律。
 * @param controller_linear 对象。
 * @param name 名称。
 * @param k 斜率（duty / (m/s)）。
 * @param b 截距或摩擦补偿幅值（duty）。
 * @param is_defriction_enabled 是否把 b 作为摩擦补偿。
 * @param max_target 输入上限（m/s）。
 * @param min_target 输入下限（m/s）。
 * @param max_out 输出上限（duty）。
 * @param min_out 输出下限（duty）。
 * @param max_step 单周期最大变化（duty）。
 * @param min_step 单周期最小变化（duty）。
 * @param deadzone 输出保持带（duty），围绕上次输出。
 * @note 初始化会校验参数范围与有限性。
 */
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

/** @} */

#endif //LAYERED_EMBEDDED_C_CONTROLLER_LINEAR_H
