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
 * @brief 初始化并联求和控制器并复制子控制律表。
 * @param controller_h 待初始化的控制器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖控制器对象。
 * @param controller_stage_h 非空子控制律指针数组；前 stage_count 项必须非空、已初始化且互不重复。
 * @param stage_count 子控制律数量，范围 1..CONTROLLER_COMPOSE_CAPACITY。
 * @pre 子控制律均不能指向 controller_h->base；所有指针表项在对象使用期间保持有效。
 * @post 子控制律表复制入对象、剩余槽位清零、数量已设置，并对全部子控制器执行 reset。
 * @note Debug 构建用 assert 检查自引用、重复项和空回调；仅允许初始化阶段单线程调用。
 */
void Ctrl_Compose_Init(
  controller_compose_t      *controller_h,
  const char                *name,
  controller_base_t *const  *controller_stage_h,
  uint8_t                    stage_count
);

/** @} */

#endif //LAYERED_EMBEDDED_C_CONTROLLER_COMPOSE_H
