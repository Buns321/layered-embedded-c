/**
 * @file platform_dac_backend.h
 * @brief STM32 系列公共 DAC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H

#include "platform_dac.h"
#include "platform_def.h"
#include <stdbool.h>

/**
 * @brief 查询 DAC 实例是否映射到板级启用的 HAL 句柄。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @retval true 实例有效且映射到 DAC HAL 句柄。
 * @retval false 枚举值非法或没有 HAL 句柄。
 * @post 只执行映射查询，不启动通道、不写输出寄存器。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_dac_backend_is_valid(platform_dac_instance_t dac);

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
);

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
);

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
);

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
);

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
);

/**
 * @brief 读取并映射 DAC 后端当前错误位。
 * @param dac DAC 实例枚举值；当前仅 PLATFORM_DAC_1 有定义。
 * @return platform_dac_error_t 位掩码；可组合 DMA 欠载、DMA、超时和未知错误位；
 *         实例非法时返回 UNKNOWN。
 * @post 只读取 HAL 错误快照，不清除错误、不停止通道，也不修改输出数据。
 * @note 可在任务或中断上下文调用；读取不会触发 DMA 欠载恢复。
 */
uint32_t platform_dac_backend_get_error(platform_dac_instance_t dac);

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
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_BACKEND_H
