//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_base.c
 * @brief 滤波器抽象接口实现。
 * @ingroup filter
 */

#include "filter_base.h"
#include <assert.h>
#include <stddef.h>

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
filter_status_t Filter_Base_Construct(
  filter_base_t *filter_h,
  const filter_config_t *config,
  const filter_ops_t *ops
) {
  if (
    filter_h == NULL ||
    config == NULL ||
    config->name == NULL ||
    ops == NULL
  ) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  if (ops->init != NULL) {
    filter_status_t status = ops->init(filter_h, config);
    if (status != FILTER_STATUS_OK) {
      return status;
    }
  }

  filter_h->name = config->name;
  filter_h->ops  = ops;

  if (ops->reset != NULL) {
    ops->reset(filter_h);
  }
  return FILTER_STATUS_OK;
}

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
void Filter_Base_Update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output) {
  assert(
    filter_h              &&
    filter_h->ops         &&
    input             &&
    output            &&
    input != output
  );
  *output = *input;
  Filter_Sample_Sanitize(output);
  if (filter_h->ops->update != NULL) {
    filter_h->ops->update(filter_h, output);
  }
  Filter_Sample_Sanitize(output);

  assert(output->valid_mask == (output->valid_mask & FILTER_CHANNEL_MASK));
  output->time_ms = input->time_ms;
}

/**
 * @brief 复位滤波器内部状态。
 * @param filter_h 已初始化的滤波器对象，不能为 NULL。
 * @pre filter_h 已构造，且当前没有并发 update。
 * @post 具体滤波器内部状态被清空；名称、ops 和静态配置保持不变。
 * @note 没有 reset 回调时为空操作；单写者调用，不分配动态内存，不改变滤波器拓扑。
 */
void Filter_Base_Reset(filter_base_t *filter_h) {
  assert(filter_h && filter_h->ops);
  if (filter_h->ops->reset != NULL) {
    filter_h->ops->reset(filter_h);
  }
}
