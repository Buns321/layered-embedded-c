/**
 * @file filter_base.h
 * @brief 滤波器抽象接口、预复制清洗契约与通用构造器。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_BASE_H
#define LAYERED_CAR_FILTER_BASE_H

#include "filter_sample.h"
#include <stddef.h>

typedef struct filter_base_s filter_base_t;

/**
 * @brief Filter 公共接口状态。
 */
typedef enum filter_status_e {
  FILTER_STATUS_OK = 0U,
  FILTER_STATUS_INVALID_ARGUMENT,
  FILTER_STATUS_INVALID_CONFIG
} filter_status_t;

/**
 * @brief Filter 构造器公共配置（data 由具体滤波器 ops->init 解释，可为 NULL）。
 */
typedef struct filter_config_s {
  const char *name;
  const void *data;
} filter_config_t;

/**
 * @brief 滤波器虚表；任一回调可为 NULL（分别表示无派生配置、透传、无状态）。
 */
typedef struct filter_ops_s {
  filter_status_t (*init)(filter_base_t *filter_h, const filter_config_t *config);
  void (*update)(filter_base_t *filter_h, filter_sample_t *sample);
  void (*reset)(filter_base_t *filter_h);
} filter_ops_t;

/**
 * @brief 滤波器抽象基类；须作为具体滤波器结构体首成员。
 */
typedef struct filter_base_s {
  const filter_ops_t *ops;
  const char *name;
} filter_base_t;

/**
 * @brief 执行派生校验、绑定 name/ops 并复位状态；失败时不修改对象。
 */
filter_status_t Filter_Base_Construct(filter_base_t *filter_h, const filter_config_t *config,
                                      const filter_ops_t *ops);

/**
 * @brief 预复制并清洗 input 到 output，调用派生 update 后再次清洗并恢复 time_ms。
 */
void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input,
                        filter_sample_t *output);

/**
 * @brief 复位滤波器内部状态（保留配置参数与绑定关系）。
 */
void Filter_Base_Reset(filter_base_t *filter_h);

/** @} */

#endif /* LAYERED_CAR_FILTER_BASE_H */
