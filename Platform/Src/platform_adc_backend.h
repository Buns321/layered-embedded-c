/**
 * @file platform_adc_backend.h
 * @brief STM32 系列公共 ADC 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H

#include "platform_adc.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 查询 ADC 实例是否已映射到板级启用的 HAL 句柄。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval true 实例已启用且映射到 HAL 句柄。
 * @retval false 枚举值越界、实例未启用或缺少 HAL 句柄。
 * @post 只执行映射查询，不启动转换、不访问数据寄存器，也不修改错误快照。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_adc_backend_is_valid(platform_adc_instance_t adc);

/**
 * @brief 查询 ADC 实例是否具有独立 DMA 请求能力。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval true 实例有效，且当前 MCU 系列支持该实例的 DMA 启动请求。
 * @retval false 实例非法、未启用或该实例没有独立 DMA 请求。
 * @post 只执行能力查询，不启动 DMA、不修改错误快照。
 * @note STM32F1 仅 ADC1 返回 true；STM32F4 对所有有效实例返回 true。
 */
bool platform_adc_backend_supports_dma(platform_adc_instance_t adc);

/**
 * @brief 以轮询模式启动规则组转换。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval PLATFORM_STATUS_OK 转换已启动。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 ADC 仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre MX_ADCx_Init() 已完成，规则组通道和采样时间已由 CubeMX 配置。
 * @post 成功启动后由调用方继续轮询完成；本函数不读取转换结果，也不拥有调用方缓冲区。
 * @note 本函数不执行 CubeMX 结构性初始化；可能需要等待硬件状态，但不会分配动态内存。
 */
platform_status_t platform_adc_backend_start(platform_adc_instance_t adc);

/**
 * @brief 停止轮询模式下的 ADC 转换。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval PLATFORM_STATUS_OK 转换已停止。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 ADC 仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @post 成功后 ADC 不再产生新的轮询转换结果；已读取的数据寄存器内容不保证保留。
 * @note 不负责关闭 CubeMX 配置的时钟或复用；不分配动态内存。
 */
platform_status_t platform_adc_backend_stop(platform_adc_instance_t adc);

/**
 * @brief 阻塞等待当前 ADC 规则组转换完成。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @param timeout 超时时间，单位毫秒；0 表示立即检查，PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 已观察到转换完成。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 到期前未完成转换。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告转换失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre 转换已启动，且本函数在任务上下文调用。
 * @post 只等待完成，不读取或清除数据寄存器，也不停止转换。
 * @note 实现通过轮询等待，不主动休眠；不得在中断上下文调用。
 */
platform_status_t platform_adc_backend_poll(
  platform_adc_instance_t adc,
  uint32_t timeout
);

/**
 * @brief 读取 ADC 最近一次规则组转换的原始码值。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @param value 输出指针，接收原始 ADC 码值；调用方拥有存储，非 NULL，范围由 CubeMX 配置的
 *              数据位宽决定（通常为 0..4095）。
 * @retval PLATFORM_STATUS_OK 已将寄存器值写入 *value。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法、未启用、无 HAL 句柄或 value 为 NULL。
 * @pre value 指向至少一个 uint32_t 的可写存储，且生命周期覆盖本次调用。
 * @post 本函数不等待 EOC、不停止转换，也不修改错误快照；调用方必须先确认转换完成。
 * @note 读取的是硬件数据寄存器内容，不转换为伏特等物理量。
 */
platform_status_t platform_adc_backend_get_value(
  platform_adc_instance_t adc,
  uint32_t *value
);

/**
 * @brief 启动 ADC 规则组中断转换。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval PLATFORM_STATUS_OK HAL 已接受转换请求，完成事件随后由中断回调上报。
 * @retval PLATFORM_STATUS_BUSY ADC 已有转换在运行或 HAL 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre MX_ADCx_Init() 已完成；需要事件通知时，公共层回调已在启动前注册。
 * @post 成功返回仅表示请求已接受，不表示转换完成；完成和错误通过 HAL 回调分发。
 * @note 异步接口不接收缓冲区；回调运行在中断上下文，不得阻塞或分配内存。
 */
platform_status_t platform_adc_backend_start_it(
  platform_adc_instance_t adc
);

/**
 * @brief 停止 ADC 规则组中断转换。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval PLATFORM_STATUS_OK 中断转换已停止。
 * @retval PLATFORM_STATUS_BUSY ADC 或 HAL 正忙，停止请求未被接受。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @post 成功后不再产生新的转换完成中断；已进入中断的事件仍可能先于本函数返回完成分发。
 * @note 本函数不等待在途回调结束，也不负责释放任一缓冲区。
 */
platform_status_t platform_adc_backend_stop_it(
  platform_adc_instance_t adc
);

/**
 * @brief 启动 ADC 规则组 DMA 转换。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @param data DMA 目标数组，接收原始 ADC 码值；调用方拥有该数组，非 NULL，且不会被本层复制。
 * @param length 转换次数（不是字节数），必须大于 0。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 请求。
 * @retval PLATFORM_STATUS_BUSY ADC 或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre CubeMX 已配置 DMA 方向和模式；data 非 NULL、length 大于 0，且公共层已校验实例能力；
 *      在 STM32F1 上公共层仅允许 ADC1 使用本接口。
 * @post 成功返回后 DMA 会写入 data；直到完成、错误或显式停止前，data 必须保持有效且不得被调用方并发改写。
 * @note 成功返回不表示转换完成；多通道扫描必须使用 DMA，半完成事件只对应已完成的那一半。
 */
platform_status_t platform_adc_backend_start_dma(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
);

/**
 * @brief 停止 ADC 规则组 DMA 转换。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @retval PLATFORM_STATUS_OK DMA 转换已停止。
 * @retval PLATFORM_STATUS_BUSY ADC 或 DMA 正忙，停止请求未被接受。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @post 成功后本层不再写入原先的 DMA 目标数组；调用方可在确认无在途事件后复用该存储。
 * @note 不释放缓冲区，也不重新启动转换；不分配动态内存。
 */
platform_status_t platform_adc_backend_stop_dma(
  platform_adc_instance_t adc
);

/**
 * @brief 读取并映射 ADC 后端当前错误位。
 * @param adc ADC 实例枚举值；有效值由 platform_adc_instance_t 定义。
 * @return platform_adc_error_t 位掩码；NONE 表示无错误，UNKNOWN 表示实例非法或 HAL 返回
 *         未定义错误位，其余位可组合出现。
 * @post 只读取 HAL 错误快照，不清除错误、不停止转换，也不修改转换数据。
 * @note 可在任务或中断上下文调用；若实例非法则返回 PLATFORM_ADC_ERROR_UNKNOWN。
 */
uint32_t platform_adc_backend_get_error(platform_adc_instance_t adc);

/**
 * @brief 把 ADC 转换事件分发给已注册回调。
 * @param adc ADC 实例；非法或未注册时无操作。
 * @param type 事件类型；ERROR 会附带当前后端错误位，其他事件错误位为 NONE。
 * @post 已注册且 callback 非 NULL 时同步调用一次；事件对象只存在于本次回调期间。
 * @warning 由 HAL 中断回调调用，执行上下文为中断；不得阻塞或分配内存。
 * @note 不保留事件对象、不复制 context；回调由注册阶段保存。
 */
void platform_adc_dispatch_event(
  platform_adc_instance_t adc,
  platform_adc_event_type_t type
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_BACKEND_H
