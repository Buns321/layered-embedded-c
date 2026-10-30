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
typedef struct controller_linear_s {
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
 * @param controller_h 待初始化的线性控制律存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖控制律对象。
 * @param k 输入到输出的线性斜率，单位 duty/(m/s)，必须有限。
 * @param b 截距或摩擦补偿幅值，单位 duty，必须有限。
 * @param is_defriction_enabled true 时按线性输出符号施加 b，false 时始终加 b。
 * @param max_target 输入上限，单位 m/s，必须有限且不小于 min_target。
 * @param min_target 输入下限，单位 m/s，必须有限且不大于 max_target。
 * @param max_out 输出上限，单位 duty，必须有限且非负。
 * @param min_out 输出下限，单位 duty，必须有限且非正。
 * @param max_step 单次更新最大变化量，单位 duty，必须有限且非负、不小于 min_step。
 * @param min_step 单次更新最小变化量，单位 duty，必须有限且非负、不大于 max_step。
 * @param deadzone 围绕 last_out 的输出保持带，单位 duty，必须有限、非负且小于 max_step。
 * @pre 所有浮点参数必须有限；范围不变量由 Debug 构建 assert 检查。
 * @post 控制律参数和 ops 已绑定，last_out 清零；不分配动态内存。
 * @note 仅允许初始化阶段单线程调用，不得与更新或复位并发。
 */
void Ctrl_Linear_Init(
  controller_linear_t *controller_h,
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
