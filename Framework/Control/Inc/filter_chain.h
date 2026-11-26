/**
 * @file filter_chain.h
 * @brief 固定容量串联滤波器链。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，采用静态数组保存非拥有
 *          子滤波器指针，使用栈上双缓冲（ping-pong）依次推进各级滤波，零动态分配。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_CHAIN_H
#define LAYERED_CAR_FILTER_CHAIN_H

#include "filter_base.h"
#include <stdint.h>

/** @brief 串联滤波器链支持的最大级数（无量纲）。 */
#define FILTER_CHAIN_CAPACITY 8U

/**
 * @brief 串联滤波器链静态对象（base 必须为首成员，stages 非拥有指针）。
 */
typedef struct filter_chain_s {
  filter_base_t base;                           /**< 多态基类（必须为首成员）。 */
  filter_base_t *stages[FILTER_CHAIN_CAPACITY]; /**< 子滤波器非拥有句柄数组。 */
  uint8_t stage_count; /**< 有效串联级数（范围：1..FILTER_CHAIN_CAPACITY）。 */
} filter_chain_t;

/**
 * @brief 初始化串联滤波器链（stage_count 须在 1..FILTER_CHAIN_CAPACITY 且无重复/自引用）。
 * @param[out] filter_h    待初始化的滤波器链对象指针（非 NULL）。
 * @param[in]  name        滤波器链名称字符串（非 NULL）。
 * @param[in]  stages      子滤波器基类指针数组（非 NULL，长度须 >= stage_count）。
 * @param[in]  stage_count 串联级数（范围：1..FILTER_CHAIN_CAPACITY）。
 * @return 初始化状态码。
 * @retval FILTER_STATUS_OK               全部级联检查通过并完成初始化与复位。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h、name 或 stages 为 NULL。
 * @retval FILTER_STATUS_INVALID_CONFIG   stage_count 越界、存在空项/自引用/未初始化项或重复指针。
 * @pre 所有 stages[i] 须已完成各自 Init 初始化，且不得包含 filter_h 本身。
 */
filter_status_t Filter_Chain_Init(filter_chain_t *filter_h, const char *name,
                                  filter_base_t *const *stages, uint8_t stage_count);

#endif /* LAYERED_CAR_FILTER_CHAIN_H */

/** @} */
