//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_passthrough.h
 * @brief 透传滤波器。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H
#define LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H

#include "filter_base.h"

/**
 * @brief 透传滤波器对象。
 * @note 依赖 Filter_Base_Update 的预复制语义，直接输出原始通道。
 */
typedef struct filter_passthrough_s {
  filter_base_t filter_base;
} filter_passthrough_t;

/**
 * @brief 初始化透传滤波器。
 * @param filter_h 待初始化的透传滤波器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @retval FILTER_STATUS_OK 参数有效，滤波器已绑定透传回调。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 或 name 为空。
 * @pre filter_h 可写；name 在滤波器对象存活期间保持有效。
 * @post 成功时滤波器使用空回调操作表，由 base 完成预复制和清洗；失败时不修改对象。
 * @note 仅在初始化阶段调用；不分配动态内存。
 */
filter_status_t Filter_Passthrough_Init(filter_passthrough_t *filter_h, const char *name);

/** @} */

#endif // LAYERED_EMBEDDED_C_FILTER_PASSTHROUGH_H
