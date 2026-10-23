//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file controller_compose.h
 * @brief 并联求和控制器：把多个子控制律的输出相加。
 * @addtogroup controller
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H
#define LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H

#include "controller_base.h"
#include <stdint.h>

#define CONTROLLER_COMPOSE_CAPACITY 8u

/**
 * @brief 并联求和控制器对象。
 * @note 所有子控制器接收相同的 target/measure/dt，输出相加。
 * @note 子控制器表在 Init 时复制；固定容量，不使用动态内存。
 */
typedef struct controller_compose_s {
  controller_base_t  base;
  controller_base_t *controller_stage_h[CONTROLLER_COMPOSE_CAPACITY];
  uint8_t            stage_count;
} controller_compose_t;

/**
 * @brief 初始化 compose 控制器。
 * @param controller_h 对象。
 * @param name 名称。
 * @param controller_stage_h 子控制器静态表。
 * @param stage_count 子控制器数量，范围 1..CONTROLLER_COMPOSE_CAPACITY。
 * @note Init 会校验空指针、自引用和重复项，并级联 reset。
 * @note 任一子控制器返回 ERROR 时，整体输出 0 并返回 ERROR。
 */
void Ctrl_Compose_Init(
  controller_compose_t      *controller_h,
  const char                *name,
  controller_base_t *const  *controller_stage_h,
  uint8_t                    stage_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H
