//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_base.h
 * @brief 滤波器抽象接口与预复制语义。
 * @addtogroup filter
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_BASE_H
#define LAYERED_EMBEDDED_C_FILTER_BASE_H

#include "filter_sample.h"
#include <stddef.h>

typedef struct filter_base_s filter_base_t;

/**
 * @brief Filter 公共接口状态。
 */
typedef enum filter_status_e {
  FILTER_STATUS_OK = 0U,
  FILTER_STATUS_INVALID_ARGUMENT,
  FILTER_STATUS_INVALID_CONFIG,
} filter_status_t;

/**
 * @brief Filter 构造器公共配置。
 * @note data 的具体类型和生命周期由对应 filter_ops_t.init 定义，允许为 NULL。
 */
typedef struct filter_config_s {
  const char *name;
  const void *data;
} filter_config_t;

/**
 * @brief 滤波器操作表。
 * @note 所有回调均可为 NULL。init 在对象仍未修改时校验配置并初始化具体字段，
 *       失败前不得写入对象；update 接收已预复制并清洗的样本；reset 清除状态。
 */
typedef struct filter_ops_s {
  filter_status_t (*init)(filter_base_t *filter_h, const filter_config_t *config);
  void (*update)(filter_base_t *filter_h, filter_sample_t *sample);
  void (*reset)(filter_base_t *filter_h);
} filter_ops_t;

/**
 * @brief 滤波器抽象对象。
 */
typedef struct filter_base_s {
  const filter_ops_t *ops;
  const char *name;
} filter_base_t;

/**
 * @brief 通过公共配置和操作表构造滤波器。
 * @param filter_h 具体滤波器对象的基类视图，必须是对象起始地址，不能为 NULL。
 * @param config 非空构造配置；name 非空，data 类型必须与 ops 对应。
 * @param ops 非空操作表；各回调可为 NULL，生命周期必须覆盖滤波器对象。
 * @retval FILTER_STATUS_OK 配置有效，具体对象已初始化、绑定 base 并 reset。
 * @retval FILTER_STATUS_INVALID_ARGUMENT 公共指针、name、ops 或派生参数非法。
 * @retval FILTER_STATUS_INVALID_CONFIG init 拒绝了派生配置。
 * @pre filter_h 指向具体对象首成员；config、data 引用的存储和 ops 在调用期间
 *      可读，且不得与 filter_h 覆盖的对象存储重叠。
 * @post 成功时先执行 init，再绑定 name/ops，最后调用 reset；失败时不修改对象。
 * @note 仅允许初始化阶段单线程调用；init 必须在首次写入前完成全部可失败检查。
 */
filter_status_t Filter_Base_Construct(filter_base_t *filter_h, const filter_config_t *config,
                                      const filter_ops_t *ops);
/**
 * @brief 运行一次滤波。
 * @param filter_h 已初始化的滤波器对象，不能为 NULL。
 * @param input 只读输入样本；超出 FILTER_CHANNEL_CAPACITY 的掩码位会在复制后清除。
 * @param output 调用方拥有的输出样本，不能与 input 别名。
 * @pre filter_h 已构造；input 与 output 在调用期间可访问。
 * @post output 先完整复制 input，并清除非法掩码位和非有限有效通道；随后把清洗后的
 *       output 原地交给具体滤波器。没有 update 回调时保持清洗后的预复制结果。
 *       返回前再次清洗并恢复输入 time_ms。
 * @note 同一实例单写者调用，不分配动态内存；输出通道值是否代表物理量由具体滤波器定义。
 */
void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input,
                        filter_sample_t *output);
/**
 * @brief 复位滤波器内部状态。
 * @param filter_h 已初始化的滤波器对象，不能为 NULL。
 * @pre filter_h 已构造，且当前没有并发 update。
 * @post 具体滤波器内部状态被清空；名称、ops 和静态配置保持不变。
 * @note 没有 reset 回调时为空操作；单写者调用，不分配动态内存，不改变滤波器拓扑。
 */
void Filter_Base_Reset(filter_base_t *filter_h);

/** @} */

#endif // LAYERED_EMBEDDED_C_FILTER_BASE_H
