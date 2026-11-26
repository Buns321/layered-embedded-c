/**
 * @file filter_base.h
 * @brief 滤波器抽象接口、预复制清洗契约与通用构造器。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，采用静态多态基类模式。
 *          Filter_Base_Update() 统一承担输入预复制、前后两次非有限值清洗（Sanitize）
 *          以及输入时间戳透传，使具体滤波器实现可专注于纯数学变换。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_BASE_H
#define LAYERED_CAR_FILTER_BASE_H

#include "filter_sample.h"
#include <stddef.h>

/** @brief 滤波器抽象基类前置声明。 */
typedef struct filter_base_s filter_base_t;

/**
 * @brief Filter 公共接口状态枚举。
 */
typedef enum filter_status_e {
  FILTER_STATUS_OK = 0U,          /**< 操作成功完成。 */
  FILTER_STATUS_INVALID_ARGUMENT, /**< 传入了 NULL 指针或非法地址参数。 */
  FILTER_STATUS_INVALID_CONFIG    /**< 配置参数越界、非有限或存在结构冲突。 */
} filter_status_t;

/**
 * @brief Filter 构造器公共配置（data 由具体滤波器 ops->init 解释，可为 NULL）。
 */
typedef struct filter_config_s {
  const char *name; /**< 滤波器实例名称字符串（非 NULL）。 */
  const void *data; /**< 指向具体滤波器派生配置的不透明只读指针（可为 NULL）。 */
} filter_config_t;

/**
 * @brief 滤波器虚表；任一回调可为 NULL（分别表示无派生配置、透传、无状态）。
 */
typedef struct filter_ops_s {
  /**
   * @brief 派生滤波器初始化回调（可为 NULL）。
   * @param[in,out] filter_h 滤波器基类句柄（非 NULL）。
   * @param[in]     config   构造器公共配置指针（非 NULL）。
   * @return 初始化状态枚举。
   */
  filter_status_t (*init)(filter_base_t *filter_h, const filter_config_t *config);
  /**
   * @brief 原地单步滤波更新回调（可为 NULL，表示透传）。
   * @param[in,out] filter_h 滤波器基类句柄（非 NULL）。
   * @param[in,out] sample   已预复制并清洗的样本指针（非 NULL）。
   */
  void (*update)(filter_base_t *filter_h, filter_sample_t *sample);
  /**
   * @brief 复位滤波器内部状态回调（可为 NULL，表示无状态）。
   * @param[in,out] filter_h 滤波器基类句柄（非 NULL）。
   */
  void (*reset)(filter_base_t *filter_h);
} filter_ops_t;

/**
 * @brief 滤波器抽象基类；须作为具体滤波器结构体首成员（禁止按值拷贝）。
 */
typedef struct filter_base_s {
  const filter_ops_t *ops; /**< 指向静态只读滤波器虚表的非拥有指针。 */
  const char *name;        /**< 滤波器实例名称字符串（生命周期须覆盖实例全程）。 */
} filter_base_t;

/**
 * @brief 执行派生校验、绑定 name/ops 并复位状态；失败时不修改对象。
 * @param[out] filter_h 待构造的滤波器基类句柄（非 NULL）。
 * @param[in]  config   构造器配置指针（非 NULL，且 config->name 非 NULL）。
 * @param[in]  ops      静态只读虚表指针（非 NULL）。
 * @return 构造结果状态码。
 * @retval FILTER_STATUS_OK               构造与内部复位全部成功。
 * @retval FILTER_STATUS_INVALID_ARGUMENT 参数指针为空。
 * @retval FILTER_STATUS_INVALID_CONFIG   派生 ops->init 返回配置错误。
 */
filter_status_t Filter_Base_Construct(filter_base_t *filter_h, const filter_config_t *config,
                                      const filter_ops_t *ops);

/**
 * @brief 预复制并清洗 input 到 output，调用派生 update 后再次清洗并恢复 time_ms。
 * @param[in,out] filter_h 滤波器基类句柄（非 NULL，且已绑定有效 ops）。
 * @param[in]     input    输入样本指针（非 NULL，不可与 output 混叠）。
 * @param[out]    output   输出样本指针（非 NULL，不可与 input 混叠）。
 * @note 若任一指针为 NULL 或 input == output，函数静默返回且不修改 output。
 */
void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input,
                        filter_sample_t *output);

/**
 * @brief 复位滤波器内部状态（保留配置参数与绑定关系）。
 * @param[in,out] filter_h 滤波器基类句柄（为 NULL 或未绑定 reset 回调时静默返回）。
 */
void Filter_Base_Reset(filter_base_t *filter_h);

#endif /* LAYERED_CAR_FILTER_BASE_H */

/** @} */
