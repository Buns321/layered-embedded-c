/**
 * @file filter_passthrough.h
 * @brief 多通道透传滤波器（复用 base 预复制与非有限清洗语义）。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          不改变有效通道数值，仅借用 Filter_Base_Update() 的清洗契约剔除 NaN/Inf
 *          通道并保持时间戳一致，常用于默认占位或纯数据清洗节点。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_PASSTHROUGH_H
#define LAYERED_CAR_FILTER_PASSTHROUGH_H

#include "filter_base.h"

/**
 * @brief 透传滤波器对象（base 必须为首成员）。
 */
typedef struct filter_passthrough_s {
  filter_base_t base; /**< 多态基类（必须为首成员）。 */
} filter_passthrough_t;

/**
 * @brief 初始化透传滤波器。
 * @param[out] filter_h 待初始化的透传滤波器指针（非 NULL）。
 * @param[in]  name     滤波器名称字符串（非 NULL）。
 * @return 初始化状态码。
 * @retval FILTER_STATUS_OK               初始化成功。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 或 name 为 NULL。
 */
filter_status_t Filter_Passthrough_Init(filter_passthrough_t *filter_h, const char *name);

#endif /* LAYERED_CAR_FILTER_PASSTHROUGH_H */

/** @} */
