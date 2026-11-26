/**
 * @file filter_passthrough.h
 * @brief 多通道透传滤波器（复用 base 预复制与非有限清洗语义）。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_PASSTHROUGH_H
#define LAYERED_CAR_FILTER_PASSTHROUGH_H

#include "filter_base.h"

/**
 * @brief 透传滤波器对象（base 必须为首成员）。
 */
typedef struct filter_passthrough_s {
  filter_base_t base;
} filter_passthrough_t;

/**
 * @brief 初始化透传滤波器。
 */
filter_status_t Filter_Passthrough_Init(filter_passthrough_t *filter_h, const char *name);

/** @} */

#endif /* LAYERED_CAR_FILTER_PASSTHROUGH_H */
