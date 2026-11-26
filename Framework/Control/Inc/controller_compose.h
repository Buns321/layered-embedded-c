/**
 * @file controller_compose.h
 * @brief 并联求和控制律：将多个子控制律对相同输入的输出相加。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，采用静态数组保存非拥有
 *          子控制律句柄，适用于前馈 + 反馈（如 Linear + PID）并联复合控制。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_CONTROLLER_COMPOSE_H
#define LAYERED_CAR_CONTROLLER_COMPOSE_H

#include "controller_base.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief 并联组合控制律支持的最大子控制律级数（无量纲）。 */
#define CONTROLLER_COMPOSE_CAPACITY 8U

/**
 * @brief 并联求和控制律静态对象（base 必须为首成员，stages 非拥有指针）。
 */
typedef struct controller_compose_s {
  controller_base_t base;                                 /**< 多态基类（必须为首成员）。 */
  controller_base_t *stages[CONTROLLER_COMPOSE_CAPACITY]; /**< 子控制律非拥有句柄数组。 */
  uint8_t stage_count; /**< 有效子控制律数量（1..CONTROLLER_COMPOSE_CAPACITY）。 */
} controller_compose_t;

/**
 * @brief 校验子控制律表并初始化组合控制器（拒绝自引用、空项与重复项）。
 * @param[out] compose     待初始化的组合控制律对象指针（非 NULL）。
 * @param[in]  name        控制律名称字符串（非 NULL）。
 * @param[in]  stages      子控制律基类指针数组（非 NULL，长度须 >= stage_count）。
 * @param[in]  stage_count 子控制律数量（范围：1..CONTROLLER_COMPOSE_CAPACITY）。
 * @return 初始化是否成功。
 * @retval true  全部子控制律合法，已完成绑定与内部复位。
 * @retval false 参数为空、数量越界、存在自引用/未初始化项或重复指针（不修改 compose）。
 * @pre 所有 stages[i] 须已完成各自 Init 初始化，且不包含 compose 本身。
 */
bool Ctrl_Compose_Init(controller_compose_t *compose, const char *name,
                       controller_base_t *const *stages, uint8_t stage_count);

#endif /* LAYERED_CAR_CONTROLLER_COMPOSE_H */

/** @} */
