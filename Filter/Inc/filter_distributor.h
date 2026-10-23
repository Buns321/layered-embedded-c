//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_distributor.h
 * @brief 多路路由与通道映射。
 * @addtogroup filter_h
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H
#define LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H

#include "filter_base.h"
#include <stdint.h>

/** @brief 最多路由数。 */
#define FILTER_DISTRIBUTOR_CAPACITY 8u
/** @brief 单条路由最多输入/输出通道数。 */
#define FILTER_ROUTE_CAPACITY       4u

/**
 * @brief 一条路由：把全局输入通道映射到某个滤波器的局部输入，
 *        再把该滤波器的局部输出映射回全局输出通道。
 */
typedef struct filter_distributor_route_s {
  filter_base_t *filter_h;
  uint8_t input_index[FILTER_ROUTE_CAPACITY];
  uint8_t output_index[FILTER_ROUTE_CAPACITY];
  uint8_t input_count;
  uint8_t output_count;
} filter_distributor_route_t;

/**
 * @brief 多路分发器对象。
 * @note 路由输出会覆盖 base 预复制进来的通道；跨路由写同一输出通道在 Init 阶段拒绝。
 */
typedef struct filter_distributor_s {
  filter_base_t filter_base;
  filter_distributor_route_t routes[FILTER_DISTRIBUTOR_CAPACITY];
  uint8_t route_count;
} filter_distributor_t;

/**
 * @brief 初始化分发器。
 * @param filter_h 对象。
 * @param name 名称。
 * @param routes 静态路由表。
 * @param route_count 路由数，范围 1..FILTER_DISTRIBUTOR_CAPACITY。
 * @note Init 会校验重复滤波器、重复通道、非法索引和跨路由输出冲突。
 */
void Filter_Distributor_Init(filter_distributor_t *filter_h, const char *name, const filter_distributor_route_t *routes, uint8_t route_count);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H
