/**
 * @file controller_linear.h
 * @brief 线性前馈/开环控制律：线性映射、摩擦符号补偿、步长限幅与保持带死区。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          输出计算式为 result = target * k + bias，支持静摩擦符号补偿、输出限幅、
 *          单步斜率限幅（max_step）、最小步长跳变（min_step）及保持带死区（deadzone）。
 *
 * @addtogroup Framework_Control
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
  float k;                 /**< 线性前馈增益（单位：duty/(m/s)，须为有限值）。 */
  float b;                 /**< 偏置/静摩擦补偿量（单位：duty，须为有限值）。 */
  bool defriction_enabled; /**< true 时偏置乘以 target*k 的符号以补偿正反静摩擦。 */
  float max_target;        /**< 目标上限（单位：m/s；0 且 min_target==0 表示不限制输入）。 */
  float min_target;        /**< 目标下限（单位：m/s；0 且 max_target>0 时默认取 -max_target）。 */
  float max_out;           /**< 输出上限（单位：duty，必填 > 0）。 */
  float min_out;           /**< 输出下限（单位：duty，<= 0；0 时默认取 -max_out）。 */
  float max_step; /**< 单步最大变化量（单位：duty，>= 0；0 表示不限制，此时 min_step 须为 0）。 */
  float min_step; /**< 非零变化时的最小步长（单位：duty，>= 0 且 <= max_step）。 */
  float deadzone; /**< 相对 last_out 的保持带死区半径（单位：duty，>= 0；启用步长限制时须 <
                     max_step）。 */
} ctrl_linear_cfg_t;

/**
 * @brief 线性控制律静态实例对象（base 必须为首成员）。
 */
typedef struct controller_linear_s {
  controller_base_t base;  /**< 多态基类（必须为首成员）。 */
  float k;                 /**< 线性前馈增益（单位：duty/(m/s)）。 */
  float b;                 /**< 偏置/静摩擦补偿量（单位：duty）。 */
  bool defriction_enabled; /**< 是否启用随符号变化的静摩擦补偿。 */
  float max_target;        /**< 生效的目标上限（单位：m/s）。 */
  float min_target;        /**< 生效的目标下限（单位：m/s）。 */
  float max_out;           /**< 生效的输出上限（单位：duty）。 */
  float min_out;           /**< 生效的输出下限（单位：duty）。 */
  float max_step;          /**< 生效的单步最大变化量（单位：duty）。 */
  float min_step;          /**< 生效的最小步长（单位：duty）。 */
  float deadzone;          /**< 生效的保持带死区半径（单位：duty）。 */
  bool limit_target;       /**< 是否启用目标输入限幅。 */
  bool limit_step;         /**< 是否启用单步变化量限幅。 */
  float last_out;          /**< 上一周期输出值（单位：duty），用于步长限幅与死区保持。 */
} controller_linear_t;

/**
 * @brief 校验配置并初始化线性控制律；失败时返回 false 且不修改 linear。
 * @param[out] linear 待初始化的线性控制律实例指针（非 NULL）。
 * @param[in]  name   控制律名称字符串（非 NULL）。
 * @param[in]  cfg    配置参数结构体指针（非 NULL）。
 * @return 初始化是否成功。
 * @retval true  配置合法，实例已初始化并清零 last_out。
 * @retval false 参数为空或存在非有限值/范围冲突（linear 保持原状）。
 */
bool Ctrl_Linear_Init(controller_linear_t *linear, const char *name, const ctrl_linear_cfg_t *cfg);

#endif /* LAYERED_CAR_CONTROLLER_LINEAR_H */

/** @} */
