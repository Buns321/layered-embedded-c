/**
 * @file filter_passthrough.c
 * @brief 多通道透传滤波器实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          虚表回调全为 NULL，完全依赖 Filter_Base_Update() 的预复制与有限值清洗语义。
 * @ingroup Framework_Control
 */

#include "filter_passthrough.h"

/** @brief 透传滤波器静态只读虚表（全 NULL 回调即表示无派生配置、透传与无状态）。 */
static const filter_ops_t s_filter_ops_passthrough = {0};

filter_status_t Filter_Passthrough_Init(filter_passthrough_t *filter_h, const char *name) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_config_t config = {
      .name = name,
      .data = NULL,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_passthrough);
}
