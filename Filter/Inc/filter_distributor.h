//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_distributor.h
 * @brief 多路路由与通道映射。
 * @addtogroup filter
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
 * @param filter_h 待初始化的分发器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖分发器对象。
 * @param routes 非空路由表；前 route_count 项在调用期间只读。
 * @param route_count 路由数，范围 1..FILTER_DISTRIBUTOR_CAPACITY。
 * @pre 每条路由的输入/输出数量在 1..FILTER_ROUTE_CAPACITY 内，索引小于 FILTER_CHANNEL_CAPACITY。
 * @post 路由表复制入对象、剩余槽位清零，并 reset 全部唯一子滤波器。
 * @note Debug 构建拒绝重复滤波器、同一路由内重复索引和跨路由重复输出；仅初始化阶段调用。
 */
void Filter_Distributor_Init(filter_distributor_t *filter_h, const char *name, const filter_distributor_route_t *routes, uint8_t route_count);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_DISTRIBUTOR_H
