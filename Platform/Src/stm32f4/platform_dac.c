/**
 * @file platform_dac.c
 * @brief STM32F4 公共 DAC 实现的 HAL 后端。
 * @ingroup platform
 */

#include "dac.h"
#include "platform_dac_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"

/**
 * @brief 把平台 DAC 实例编号转换为 HAL 句柄。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @return 对应的 DAC HAL 句柄；枚举值非法时返回 NULL。
 * @post 只执行实例映射，不启动通道，也不写输出寄存器。
 * @note 内部辅助函数，可在任务或中断上下文调用；不分配动态内存。
 */
static DAC_HandleTypeDef *platform_dac_to_hal(platform_dac_instance_t dac) {
  return dac == PLATFORM_DAC_1 ? &hdac : NULL;
}

/**
 * @brief 把 HAL DAC 句柄识别为平台实例编号。
 * @param dac_h DAC HAL 句柄指针；允许为 NULL。
 * @param dac 输出指针，成功时接收对应平台实例；允许为 NULL，此时不写入。
 * @retval true 句柄属于已启用实例，且 dac 非 NULL 时已写入实例编号。
 * @retval false 句柄为 NULL、未知或 dac 为 NULL；输出未被写入。
 * @post 只做句柄比较，不修改 DAC 状态或错误快照。
 * @note 内部辅助函数，可在中断上下文调用；不分配动态内存。
 */
static bool platform_hal_to_dac(
  DAC_HandleTypeDef *dac_h,
  platform_dac_instance_t *dac
) {
  if (dac_h == &hdac && dac != NULL) {
    *dac = PLATFORM_DAC_1;
    return true;
  }
  return false;
}

/**
 * @brief 把 DAC 通道枚举转换为 HAL 通道宏。
 * @param channel 输出通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @return 对应的 DAC_CHANNEL_x HAL 宏；输入必须由调用方预先校验。
 * @pre channel 为 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @post 仅执行枚举映射，不访问 DAC 寄存器。
 * @note 非 CHANNEL_1 的值会映射为 CHANNEL_2，因此本辅助函数不替代参数校验。
 */
static uint32_t platform_dac_hal_channel(platform_dac_channel_t channel) {
  return channel == PLATFORM_DAC_CHANNEL_1 ? DAC_CHANNEL_1 : DAC_CHANNEL_2;
}

/**
 * @brief 把 DAC 对齐枚举转换为 HAL 宏。
 * @param alignment 数值对齐枚举；有效值由 platform_dac_alignment_t 定义。
 * @return 对应的 DAC_ALIGN_x HAL 宏；非法枚举回退为 DAC_ALIGN_12B_R。
 * @pre alignment 为 platform_dac_alignment_t 的已定义枚举值。
 * @post 仅执行枚举映射，不访问 DAC 寄存器。
 * @note 本辅助函数不报告参数错误；调用方必须先校验 alignment，避免静默使用回退值。
 */
static uint32_t platform_dac_hal_alignment(
  platform_dac_alignment_t alignment
) {
  switch (alignment) {
    case PLATFORM_DAC_ALIGN_8_BIT_RIGHT: return DAC_ALIGN_8B_R;
    case PLATFORM_DAC_ALIGN_12_BIT_RIGHT: return DAC_ALIGN_12B_R;
    case PLATFORM_DAC_ALIGN_12_BIT_LEFT: return DAC_ALIGN_12B_L;
    default: return DAC_ALIGN_12B_R;
  }
}

/**
 * @brief 把 HAL DAC 错误位映射为平台错误码。
 * @param hal_error HAL_DAC_GetError() 返回的 HAL 错误位掩码。
 * @return platform_dac_error_t 位掩码；无已知错误位时为 NONE，含未定义位时附加 UNKNOWN。
 * @post 只做位映射，不读取或清除 HAL 错误状态。
 * @note 内部辅助函数，可在中断上下文调用；不访问外设寄存器。
 */
static uint32_t platform_dac_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_DAC_ERROR_NONE;
  if (((hal_error & HAL_DAC_ERROR_DMAUNDERRUNCH1) != 0U) ||
      ((hal_error & HAL_DAC_ERROR_DMAUNDERRUNCH2) != 0U)) {
    error |= PLATFORM_DAC_ERROR_DMA_UNDERRUN;
  }
  if ((hal_error & HAL_DAC_ERROR_DMA) != 0U) {
    error |= PLATFORM_DAC_ERROR_DMA;
  }
  if ((hal_error & HAL_DAC_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_DAC_ERROR_TIMEOUT;
  }
  const uint32_t known_errors =
    HAL_DAC_ERROR_DMAUNDERRUNCH1 |
    HAL_DAC_ERROR_DMAUNDERRUNCH2 |
    HAL_DAC_ERROR_DMA |
    HAL_DAC_ERROR_TIMEOUT;
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_DAC_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 查询 DAC 实例是否映射到板级启用的 HAL 句柄。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @retval true 实例有效且映射到 DAC HAL 句柄。
 * @retval false 枚举值非法或没有 HAL 句柄。
 * @post 只执行映射查询，不启动通道、不写输出寄存器。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_dac_backend_is_valid(platform_dac_instance_t dac) {
  return platform_dac_to_hal(dac) != NULL;
}

/**
 * @brief 设置 DAC 通道输出寄存器，但不启动通道。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @param channel 输出通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @param alignment 输出数值对齐方式；必须与 CubeMX 的通道配置一致。
 * @param value 原始模拟码值；8 位右对齐为 0..255，12 位右/左对齐为 0..4095。
 * @retval PLATFORM_STATUS_OK 输出寄存器已写入。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 DAC 忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件拒绝写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre channel、alignment 和 value 已由公共层校验；实例有效。
 * @post 仅更新数据保持寄存器，不启动触发，也不保证引脚已输出；调用方拥有输入值。
 * @note 不执行 CubeMX 结构性初始化，也不分配动态内存。
 */
platform_status_t platform_dac_backend_set_value(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  uint32_t value
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_SetValue(
                       dac_h,
                       platform_dac_hal_channel(channel),
                       platform_dac_hal_alignment(alignment),
                       value
                     ));
}

/**
 * @brief 按 CubeMX 配置的触发方式启动 DAC 通道。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @param channel 输出通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval PLATFORM_STATUS_OK DAC 通道已启动。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 DAC 或通道忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre MX_DAC_Init() 已完成，通道触发源已由 CubeMX 配置，channel 已由公共层校验。
 * @post 成功后通道按配置的触发源输出；本层不拥有任何 DMA 缓冲区。
 * @note 不执行 CubeMX 结构性初始化，也不分配动态内存。
 */
platform_status_t platform_dac_backend_start(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Start(
                       dac_h,
                       platform_dac_hal_channel(channel)
                     ));
}

/**
 * @brief 停止 DAC 通道输出。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @param channel 输出通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval PLATFORM_STATUS_OK DAC 通道已停止。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 DAC 或通道忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre channel 已由公共层校验。
 * @post 成功后不再按触发源更新通道输出；已保持的输出电平不保证被清零。
 * @note 不关闭 CubeMX 配置的时钟或引脚，也不释放缓冲区。
 */
platform_status_t platform_dac_backend_stop(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Stop(
                       dac_h,
                       platform_dac_hal_channel(channel)
                     ));
}

/**
 * @brief 启动 DAC 通道的 DMA 样本输出。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @param channel 输出通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @param alignment 样本对齐方式；必须与 CubeMX 的通道配置一致。
 * @param data 原始 uint32_t 样本数组，非 NULL；调用方拥有，本层不复制。
 * @param length 样本数（不是字节数），必须大于 0。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 请求。
 * @retval PLATFORM_STATUS_BUSY DAC 或 DMA 忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CubeMX 已配置对应通道的 DAC DMA 和触发源；channel、alignment 已由公共层校验；
 *      data 非 NULL 且 length 大于 0。
 * @post 成功返回后 DMA 读取 data；直到完成、错误或显式停止前，data 必须保持有效且不得并发改写。
 * @note 成功返回只表示请求被接受，不表示样本已输出完；回调运行在中断上下文。
 */
platform_status_t platform_dac_backend_start_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel,
  platform_dac_alignment_t alignment,
  const uint32_t *data,
  uint32_t length
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Start_DMA(
                       dac_h,
                       platform_dac_hal_channel(channel),
                       (uint32_t *)data,
                       length,
                       platform_dac_hal_alignment(alignment)
                     ));
}

/**
 * @brief 停止 DAC 通道的 DMA 样本输出。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @param channel 输出通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @retval PLATFORM_STATUS_OK DMA 输出已停止。
 * @retval PLATFORM_STATUS_BUSY DAC 或 DMA 忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre channel 已由公共层校验。
 * @post 成功后本层不再读取原先的 data；调用方可在确认无在途事件后复用该存储。
 * @note 不释放缓冲区，也不改变 DAC 通道的静态输出配置。
 */
platform_status_t platform_dac_backend_stop_dma(
  platform_dac_instance_t dac,
  platform_dac_channel_t channel
) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_DAC_Stop_DMA(
                       dac_h,
                       platform_dac_hal_channel(channel)
                     ));
}

/**
 * @brief 读取并映射 DAC 后端当前错误位。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @return platform_dac_error_t 位掩码；可组合 DMA 欠载、DMA、超时和未知错误位；
 *         实例非法时返回 UNKNOWN。
 * @post 只读取 HAL 错误快照，不清除错误、不停止通道，也不修改输出数据。
 * @note 可在任务或中断上下文调用；读取不会触发 DMA 欠载恢复。
 */
uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac) {
  DAC_HandleTypeDef *dac_h = platform_dac_to_hal(dac);
  return dac_h == NULL ? PLATFORM_DAC_ERROR_UNKNOWN
                   : platform_dac_map_error(HAL_DAC_GetError(dac_h));
}

/**
 * @brief 把 HAL DAC 回调事件转发给已注册的项目回调。
 * @param dac_h DAC HAL 句柄指针；允许为 NULL，未知句柄时忽略。
 * @param channel 触发事件的通道，取 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2。
 * @param type 要分发的事件类型；ERROR 和 UNDERRUN 事件携带当前后端错误位，其他事件错误位为 NONE。
 * @pre channel 为 PLATFORM_DAC_CHANNEL_1 或 PLATFORM_DAC_CHANNEL_2；dac_h 允许为 NULL。
 * @post 句柄可识别时同步调用平台分发函数并向回调传递通道和错误快照；否则无操作。
 * @note 由 HAL 中断回调调用，执行上下文为中断；不得阻塞、分配内存或保存事件指针。
 */
static void platform_dac_dispatch_hal_event(
  DAC_HandleTypeDef *dac_h,
  platform_dac_channel_t channel,
  platform_dac_event_type_t type
) {
  platform_dac_instance_t dac;
  if (!platform_hal_to_dac(dac_h, &dac)) {
    return;
  }
  const platform_dac_event_t event = {
    .type = type,
    .channel = channel,
    .error = type == PLATFORM_DAC_EVENT_ERROR ||
             type == PLATFORM_DAC_EVENT_UNDERRUN
               ? platform_dac_backend_get_error(dac)
               : PLATFORM_DAC_ERROR_NONE,
  };
  platform_dac_dispatch_event(dac, &event);
}

/**
 * @brief 分发 DAC 通道 1 转换完成事件。
 * @param dac_h 触发中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 1 的 CONV_COMPLETE，错误位为 NONE；
 *       不停止通道、不清除错误，也不释放 DMA 缓冲区。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_COMPLETE
  );
}

/**
 * @brief 分发 DAC 通道 1 半转换完成事件。
 * @param dac_h 触发中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 1 的 CONV_HALF_COMPLETE，错误位为 NONE；
 *       不停止通道，也不释放 DMA 缓冲区。
 * @note 由 HAL 中断上下文调用；DMA 下只有已完成的一半样本可被回调安全读取。
 */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_CONV_HALF_COMPLETE
  );
}

/**
 * @brief 记录并分发 DAC 通道 1 错误事件。
 * @param dac_h 触发错误中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 1 的 ERROR，携带当前映射后的错误位；
 *       不清除 HAL 错误，也不自动停止或重启通道。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_DAC_ErrorCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_ERROR
  );
}

/**
 * @brief 记录并分发 DAC 通道 1 DMA 欠载事件。
 * @param dac_h 触发欠载中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 1 的 UNDERRUN，携带当前映射后的错误位；
 *       不重新填充 DMA，也不自动重启输出。
 * @note 由 HAL 中断上下文调用；恢复策略由上层在任务上下文显式执行。
 */
void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_1,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}

/**
 * @brief 分发 DAC 通道 2 转换完成事件。
 * @param dac_h 触发中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 2 的 CONV_COMPLETE，错误位为 NONE；
 *       不停止通道、不清除错误，也不释放 DMA 缓冲区。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_DACEx_ConvCpltCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_COMPLETE
  );
}

/**
 * @brief 分发 DAC 通道 2 半转换完成事件。
 * @param dac_h 触发中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 2 的 CONV_HALF_COMPLETE，错误位为 NONE；
 *       不停止通道，也不释放 DMA 缓冲区。
 * @note 由 HAL 中断上下文调用；DMA 下只有已完成的一半样本可被回调安全读取。
 */
void HAL_DACEx_ConvHalfCpltCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_CONV_HALF_COMPLETE
  );
}

/**
 * @brief 记录并分发 DAC 通道 2 错误事件。
 * @param dac_h 触发错误中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 2 的 ERROR，携带当前映射后的错误位；
 *       不清除 HAL 错误，也不自动停止或重启通道。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_DACEx_ErrorCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_ERROR
  );
}

/**
 * @brief 记录并分发 DAC 通道 2 DMA 欠载事件。
 * @param dac_h 触发欠载中断的 DAC HAL 句柄；允许为 NULL，未知句柄由内部映射函数忽略。
 * @post 句柄可识别时向已注册回调分发通道 2 的 UNDERRUN，携带当前映射后的错误位；
 *       不重新填充 DMA，也不自动重启输出。
 * @note 由 HAL 中断上下文调用；恢复策略由上层在任务上下文显式执行。
 */
void HAL_DACEx_DMAUnderrunCallbackCh2(DAC_HandleTypeDef *dac_h) {
  platform_dac_dispatch_hal_event(
    dac_h,
    PLATFORM_DAC_CHANNEL_2,
    PLATFORM_DAC_EVENT_UNDERRUN
  );
}
