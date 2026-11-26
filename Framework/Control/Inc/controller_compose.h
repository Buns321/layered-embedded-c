/**
 * @file controller_compose.h
 * @brief 并联求和控制律：将多个子控制律对相同输入的输出相加。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_CONTROLLER_COMPOSE_H
#define LAYERED_CAR_CONTROLLER_COMPOSE_H

#include "controller_base.h"
#include <stdbool.h>
#include <stdint.h>

#define CONTROLLER_COMPOSE_CAPACITY 8U

/**
 * @brief 并联求和控制律静态对象（base 必须为首成员，stages 非拥有指针）。
 */
typedef struct controller_compose_s {
  controller_base_t base;
  controller_base_t *stages[CONTROLLER_COMPOSE_CAPACITY];
  uint8_t stage_count;
} controller_compose_t;

/**
 * @brief 校验子控制律表并初始化组合控制器（拒绝自引用、空项与重复项）。
 */
bool Ctrl_Compose_Init(controller_compose_t *compose, const char *name,
                       controller_base_t *const *stages, uint8_t stage_count);

/** @} */

#endif /* LAYERED_CAR_CONTROLLER_COMPOSE_H */
