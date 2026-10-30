/**
 * @file platform_dac_common.c
 * @brief 与芯片系列无关的 DAC 参数校验、错误管理和回调分发。
 * @ingroup platform
 */

#include "platform_dac_backend.h"
#include "platform_error_internal.h"
#include <stddef.h>

static volatile uint32_t dac_error[PLATFORM_DAC_INSTANCE_COUNT];
static platform_dac_callback_binding_t dac_cb[PLATFORM_DAC_INSTANCE_COUNT];
static bool dac_cb_registered[PLATFORM_DAC_INSTANCE_COUNT];

/**
 * @brief 把 HAL DAC 结果转换为公共状态并记录错误。
 * @param dac 已通过后端校验的 DAC 实例；用于选择软件错误槽位。
 * @param status 后端返回的公共操作状态。
 * @return 原样返回 status，便于调用方直接转交。
 * @pre dac 必须小于 PLATFORM_DAC_INSTANCE_COUNT，否则会越界访问错误数组。
 * @post status 非 OK 时，将 platform_error_from_status(status) 写入该实例错误快照；
 *       成功时不修改快照。
 * @note 内部辅助函数；不分配内存、不阻塞，快照并发限制由 platform_error_record() 决定。
 */
static platform_status_t platform_dac_finish(
  platform_dac_instance_t dac,
  platform_status_t status
) {
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(
      &dac_error[dac],
      platform_error_from_status(status)
    );
  }
  return status;
}

/**
 * @brief 构造 DAC 非法参数状态。
 * @param dac 待报告的 DAC 实例；允许越界值。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post dac 位于槽位范围内时记录 PLATFORM_ERROR_INVALID_ARGUMENT；越界值不访问数组。
 * @note 内部辅助函数；不访问 DAC 硬件、不分配内存、不阻塞。
 */
static platform_status_t platform_dac_invalid(platform_dac_instance_t dac) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_record(
      &dac_error[dac],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 校验 DAC 通道枚举。
 * @param channel 待校验的输出通道。
 * @retval true 通道为 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval false 通道为其他值。
 * @post 纯函数；不修改参数或模块状态。
 * @note 内部辅助函数；不访问硬件、不分配内存。
 */
static bool platform_dac_channel_valid(platform_dac_channel_t channel) {
  return channel == PLATFORM_DAC_CHANNEL_1 ||
         channel == PLATFORM_DAC_CHANNEL_2;
}

/**
 * @brief 校验 DAC 数据对齐枚举。
 * @param alignment 待校验的数值对齐方式。
 * @retval true alignment 为 8 位右对齐、12 位右对齐或 12 位左对齐。
 * @retval false alignment 为其他值。
 * @post 纯函数；不修改参数或模块状态。
 * @note 内部辅助函数；不访问硬件、不分配内存。
 */
static bool platform_dac_alignment_valid(platform_dac_alignment_t alignment) {
  return alignment == PLATFORM_DAC_ALIGN_8_BIT_RIGHT ||
         alignment == PLATFORM_DAC_ALIGN_12_BIT_RIGHT ||
         alignment == PLATFORM_DAC_ALIGN_12_BIT_LEFT;
}

/**
 * @brief 校验 DAC 输出值范围。
 * @param alignment 已校验的数值对齐方式。
 * @param value 待校验的原始输出码值。
 * @retval true 8 位右对齐时 value 不超过 0xFF；12 位右/左对齐时不超过 0xFFF。
 * @retval false value 超出对应位宽。
 * @pre alignment 必须是 platform_dac_alignment_t 的已定义枚举值。
 * @post 纯函数；不修改参数或模块状态。
 * @note 内部辅助函数；不访问硬件、不分配内存。
 */
static bool platform_dac_value_valid(
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  return alignment == PLATFORM_DAC_ALIGN_8_BIT_RIGHT
           ? value <= 0xFFU
           : value <= 0xFFFU;
}

/**
 * @brief 把 DAC 转换事件分发给已注册回调。
 * @param dac DAC 实例；非法或未注册时无操作。
 * @param event 事件对象；可为 NULL，NULL 时无操作，非 NULL 时只在本次回调期间有效。
 * @post 已注册且 callback 非 NULL 时同步调用一次；不保存 event 指针。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 不复制事件或 context；context 由注册阶段保存，生命周期归调用方所有。
 */
void platform_dac_dispatch_event(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event
) {
  if (
    (uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT ||
    event == NULL ||
    !dac_cb_registered[dac]
  ) {
    return;
  }
  if (dac_cb[dac].callback != NULL) {
    dac_cb[dac].callback(dac, event, dac_cb[dac].context);
  }
}

/**
 * @brief 设置 DAC 输出寄存器值，但不启动通道。
 * @param dac DAC 实例；当前仅接受 PLATFORM_DAC_1。
 * @param channel DAC 输出通道；取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @param alignment 数值对齐方式；8 位右对齐时 value 上限为 0xFF，12 位右/左对齐时
 *        上限为 0xFFF；必须与通道配置匹配。
 * @param value 原始输出码值；范围由 alignment 决定，超出范围会被拒绝。
 * @retval PLATFORM_STATUS_OK 输出寄存器已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、通道、对齐或 value 非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 DAC 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件拒绝写入。
 * @pre 实例有效，且 alignment 与 CubeMX 中该通道的配置一致。
 * @post 仅更新数据保持寄存器，不启动通道，也不保证引脚已经输出该电平。
 * @note 不分配内存；调用开始时清除实例错误快照，失败时记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_DAC_SetValue(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel) ||
    !platform_dac_alignment_valid(alignment) ||
    !platform_dac_value_valid(alignment, value)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_set_value(dac, channel, alignment, value)
  );
}

/**
 * @brief 按 CubeMX 配置的触发方式启动 DAC 通道。
 * @param dac DAC 实例；当前仅接受 PLATFORM_DAC_1。
 * @param channel DAC 输出通道；取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval PLATFORM_STATUS_OK DAC 通道已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 DAC 或通道正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @pre MX_DAC_Init() 已完成，通道触发源已由 CubeMX 配置。
 * @post 成功时通道按配置的触发源输出；本层不拥有 DMA 缓冲区。
 * @note 不执行 CubeMX 结构性初始化；先调用 Platform_DAC_SetValue() 可指定静态输出值。
 */
platform_status_t Platform_DAC_Start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_start(dac, channel)
  );
}

/**
 * @brief 停止 DAC 通道。
 * @param dac DAC 实例；当前仅接受 PLATFORM_DAC_1。
 * @param channel DAC 输出通道；取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval PLATFORM_STATUS_OK DAC 通道已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 DAC 或通道正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @pre 调用方已启动或确认该通道可接受停止请求。
 * @post 成功时不再按触发源更新通道输出；已保持的输出电平不保证被清零。
 * @note 不关闭 CubeMX 配置的时钟或引脚，也不释放 DMA 缓冲区。
 */
platform_status_t Platform_DAC_Stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_stop(dac, channel)
  );
}

/**
 * @brief 启动 DAC 通道的 DMA 输出。
 * @param dac DAC 实例；当前仅接受 PLATFORM_DAC_1。
 * @param channel DAC 输出通道；取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @param alignment data 中每个样本的对齐方式；必须与 CubeMX 中的 DAC 通道配置一致。
 * @param data 原始 uint32_t 样本数组；不得为 NULL，每个元素的取值上限由 alignment 决定。
 * @param length 样本数，不是字节数，必须大于 0。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、通道、对齐、data 或 length 非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY DAC 或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @pre CubeMX 已配置对应通道的 DAC DMA 和触发源；data 在完成、错误或显式停止前保持有效。
 * @post 成功仅表示请求已接受，DMA 会读取 data；缓冲区归调用方所有且不会被复制。
 * @warning 完成、错误或停止前不得释放或并发改写 data；半完成事件只能访问已完成的那一半。
 * @note 不分配内存；失败时公共层记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_DAC_StartDMA(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel) ||
    !platform_dac_alignment_valid(alignment) ||
    data == NULL ||
    length == 0U
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_start_dma(
      dac,
      channel,
      alignment,
      data,
      length
    )
  );
}

/**
 * @brief 停止 DAC 通道的 DMA 输出。
 * @param dac DAC 实例；当前仅接受 PLATFORM_DAC_1。
 * @param channel DAC 输出通道；取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval PLATFORM_STATUS_OK DMA 输出已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法；硬件不变。
 * @retval PLATFORM_STATUS_BUSY DAC 或 DMA 正忙，停止请求未被接受。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @pre 调用方已启动 DMA 输出，并持有样本缓冲区直到确认停止完成。
 * @post 成功时本层不再读取原 data；调用方可在确认无在途事件后复用该存储。
 * @note 不释放缓冲区，也不改变 DAC 通道的静态输出配置；不分配内存。
 */
platform_status_t Platform_DAC_StopDMA(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  if ((uint32_t)dac < PLATFORM_DAC_INSTANCE_COUNT) {
    platform_error_reset(&dac_error[dac]);
  }
  if (
    !platform_dac_backend_is_valid(dac) ||
    !platform_dac_channel_valid(channel)
  ) {
    return platform_dac_invalid(dac);
  }
  return platform_dac_finish(
    dac,
    platform_dac_backend_stop_dma(dac, channel)
  );
}

/**
 * @brief 读取最近一次 HAL DAC 错误快照。
 * @param dac DAC 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT dac 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止通道、不修改输出数据，不分配内存。
 */
uint32_t Platform_DAC_GetLastError(platform_dac_instance_t dac) {
  if ((uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT) {
    return PLATFORM_ERROR_INVALID_ARGUMENT;
  }
  return dac_error[dac] | platform_dac_backend_get_error(dac);
}

/**
 * @brief 注册 DAC 回调。
 * @param dac DAC 实例；当前仅接受 PLATFORM_DAC_1。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制 callback 和 context，后续完成、半完成、错误或欠载事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，禁止阻塞或分配内存。
 * @note 应在初始化阶段、启动 DMA 输出前注册；context 的生命周期由调用方维护。
 */
platform_status_t Platform_DAC_RegisterCallback(
  platform_dac_instance_t dac,
  const platform_dac_callback_binding_t *callbacks
) {
  if (
    (uint32_t)dac >= PLATFORM_DAC_INSTANCE_COUNT ||
    callbacks == NULL ||
    !platform_dac_backend_is_valid(dac)
  ) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  if (dac_cb_registered[dac]) {
    return PLATFORM_STATUS_BUSY;
  }

  dac_cb[dac] = *callbacks;
  dac_cb_registered[dac] = true;
  return PLATFORM_STATUS_OK;
}
