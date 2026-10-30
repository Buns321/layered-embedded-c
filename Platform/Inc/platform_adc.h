//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_adc.h
 * @brief 不依赖 HAL 类型的 ADC 运行期接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ADC_H
#define LAYERED_EMBEDDED_C_PLATFORM_ADC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_ADC1 && !PLATFORM_ENABLE_ADC2 && \
    !PLATFORM_ENABLE_ADC3
#error "At least one Platform ADC instance must be enabled"
#endif

/** @brief 可用的 ADC 实例。 */
typedef enum platform_adc_instance_e {
#if PLATFORM_ENABLE_ADC1
  PLATFORM_ADC_1 = 0U,
#endif
#if PLATFORM_ENABLE_ADC2
  PLATFORM_ADC_2 = 1U,
#endif
#if PLATFORM_ENABLE_ADC3
  PLATFORM_ADC_3 = 2U,
#endif
} platform_adc_instance_t;

/** @brief ADC 身份槽位数量上限。 */
#define PLATFORM_ADC_INSTANCE_COUNT 3U

/** @brief ADC 错误标志，可同时置位多个位。 */
typedef enum platform_adc_error_e {
  PLATFORM_ADC_ERROR_NONE     = 0u,
  PLATFORM_ADC_ERROR_INTERNAL = (1u << 0),
  PLATFORM_ADC_ERROR_OVERRUN  = (1u << 1),
  PLATFORM_ADC_ERROR_DMA      = (1u << 2),
  PLATFORM_ADC_ERROR_UNKNOWN  = (1u << 7),
} platform_adc_error_t;

/** @brief ADC 异步事件。 */
typedef enum platform_adc_event_type_e {
  PLATFORM_ADC_EVENT_CONV_COMPLETE = 0U,
  PLATFORM_ADC_EVENT_CONV_HALF_COMPLETE,
  PLATFORM_ADC_EVENT_ERROR,
} platform_adc_event_type_t;

/** @brief ADC 事件数据，只在回调期间有效。 */
typedef struct platform_adc_event_s {
  platform_adc_event_type_t type;
  uint32_t error;
} platform_adc_event_t;

typedef void (*platform_adc_callback_t)(
  platform_adc_instance_t adc,
  const platform_adc_event_t *event,
  void *context
);

/**
 * @brief ADC 回调集合。
 * @note 回调在中断上下文执行。
 * @note 一个物理实例只能有一个回调所有者。
 */
typedef struct platform_adc_callback_binding_s {
  platform_adc_callback_t callback;
  void *context;
} platform_adc_callback_binding_t;

/**
 * @brief 以轮询模式启动转换。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 规则组转换已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 ADC 仍在忙；已有转换不受影响。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败；寄存器状态由 HAL 决定。
 * @pre MX_ADCx_Init() 已完成，且规则组通道已配置。
 * @post 成功返回仅表示转换已启动；完成状态须由 Platform_ADC_PollForConversion() 查询。
 * @note 不分配内存；调用开始时清除该实例的软件错误快照，失败时记录对应 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_ADC_Start(platform_adc_instance_t adc);

/**
 * @brief 停止轮询转换。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 转换已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 报告 ADC 仍在忙；停止请求未完成。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败；寄存器状态由 HAL 决定。
 * @pre 调用方已启动转换，或确认当前实例可接受停止请求。
 * @post 成功时 ADC 不再产生新的轮询转换结果；数据寄存器内容不保证保留。
 * @note 不分配内存、不关闭 CubeMX 配置的时钟或复用；调用方应串行化同一实例的启动/停止。
 */
platform_status_t Platform_ADC_Stop(platform_adc_instance_t adc);

/**
 * @brief 等待当前轮询转换完成。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param timeout 超时时间，单位 ms；0 表示立即检查，PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 已观察到规则组转换完成。
 * @retval PLATFORM_STATUS_TIMEOUT timeout 到期前仍未完成。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；不等待。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件报告转换失败。
 * @pre 转换已由 Platform_ADC_Start() 启动，且本函数在任务上下文调用。
 * @post 只等待完成，不读取或清除数据寄存器，也不停止转换。
 * @note 实现通过 HAL 轮询等待，不主动休眠；不得在中断上下文调用，不分配内存。
 */
platform_status_t Platform_ADC_PollForConversion(
  platform_adc_instance_t adc,
  uint32_t timeout
);

/**
 * @brief 读取最近一次规则组转换结果。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param value 输出地址；不得为 NULL，生命周期至少覆盖本次调用。
 * @retval PLATFORM_STATUS_OK 已将数据寄存器值写入 *value。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 value 为 NULL；*value 不变。
 * @post 成功时 *value 为原始 ADC 码值，典型范围 0..4095，实际上限由 CubeMX 数据位宽决定；
 *       失败时 *value 不变。
 * @note 不等待 EOC、不停止转换、不换算为电压或物理量；不分配内存。
 */
platform_status_t Platform_ADC_GetValue(
  platform_adc_instance_t adc,
  uint32_t *value
);

/**
 * @brief 执行一次阻塞式规则组转换序列。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param value 原始转换结果的输出地址；不得为 NULL，生命周期至少覆盖本次调用。
 * @param timeout 超时时间，单位 ms；0 表示立即检查，PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 转换完成、结果已读取，且最终停止成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或 value 为 NULL；硬件不变。
 * @retval PLATFORM_STATUS_BUSY HAL 未能启动或停止转换；寄存器状态由 HAL 决定。
 * @retval PLATFORM_STATUS_TIMEOUT 等待转换完成超时。
 * @retval PLATFORM_STATUS_ERROR 启动、转换、读取或停止失败；寄存器状态由 HAL 决定。
 * @pre CubeMX 应配置为单通道、单次转换的规则组。
 * @post 无论转换成功与否，返回前都会尝试停止 ADC；成功时 *value 为原始码值。
 * @warning 多通道扫描必须使用 DMA；中断方式只报告扫描序列最后一次转换结果。
 * @note 本函数串行执行启动、轮询、读取和停止；不分配内存、不主动休眠，应在任务上下文调用。
 */
platform_status_t Platform_ADC_Read(
  platform_adc_instance_t adc,
  uint32_t *value,
  uint32_t timeout
);

/**
 * @brief 使用规则组中断启动转换。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK HAL 已接受中断转换请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY 已有转换或 HAL 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 启动失败；寄存器状态由 HAL 决定。
 * @pre MX_ADCx_Init() 已完成；需要事件通知时，回调已在启动前注册。
 * @post 成功返回仅表示请求已接受；完成和错误通过注册的回调异步上报。
 * @note 回调在中断上下文执行，不得阻塞或分配内存；调用方应串行化同一实例的启动/停止。
 */
platform_status_t Platform_ADC_StartIT(platform_adc_instance_t adc);

/**
 * @brief 停止中断方式转换。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK 中断转换已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_BUSY ADC 或 HAL 正忙，停止请求未被接受。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败；寄存器状态由 HAL 决定。
 * @pre 中断转换已启动，且调用方已注册需要的回调。
 * @post 成功时不再产生新的转换完成中断；已进入回调的事件仍可能先于本函数返回完成分发。
 * @note 不等待在途回调结束；不分配内存，调用方应串行化同一实例的状态转换。
 */
platform_status_t Platform_ADC_StopIT(platform_adc_instance_t adc);

/**
 * @brief 启动规则组 DMA 转换。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param data DMA 目标数组；不得为 NULL，长度至少为 length 个 uint32_t。
 * @param length 转换次数（样本数），不是字节数，必须大于 0。
 * @retval PLATFORM_STATUS_OK HAL 已接受 DMA 请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例、data 或 length 非法；硬件不变。
 * @retval PLATFORM_STATUS_UNSUPPORTED 当前实例没有独立 DMA 请求；F1 上仅 ADC1 受支持。
 * @retval PLATFORM_STATUS_BUSY ADC 或 DMA 正忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @pre CubeMX 已配置 DMA 方向、模式和外设请求；data 在完成、错误或显式停止前保持有效。
 * @post 成功仅表示请求已接受，随后 DMA 会写入 data；多通道扫描时 length 应覆盖全部转换。
 * @warning 完成、错误或停止前不得释放或并发改写 data；半完成事件只能访问已完成的那一半。
 * @note data 归调用方所有且不会被复制；失败时公共层记录对应的 PLATFORM_ERROR_* 位。
 */
platform_status_t Platform_ADC_StartDMA(
  platform_adc_instance_t adc,
  uint32_t *data,
  uint32_t length
);

/**
 * @brief 停止 DMA 转换。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @retval PLATFORM_STATUS_OK DMA 转换已停止。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界或未启用；硬件不变。
 * @retval PLATFORM_STATUS_UNSUPPORTED 当前实例没有独立 DMA 请求；F1 上仅 ADC1 受支持。
 * @retval PLATFORM_STATUS_BUSY ADC 或 DMA 正忙，停止请求未被接受。
 * @retval PLATFORM_STATUS_ERROR HAL 停止失败；寄存器状态由 HAL 决定。
 * @pre 调用方已启动 DMA 转换，并持有目标缓冲区直到确认停止完成。
 * @post 成功时本层不再写入原 DMA 目标数组；调用方可在确认无在途事件后复用存储。
 * @note 不释放缓冲区、不重新启动转换；不分配内存，调用方应串行化同一实例的状态转换。
 */
platform_status_t Platform_ADC_StopDMA(platform_adc_instance_t adc);

/**
 * @brief 读取最近一次 HAL ADC 错误快照。
 * @param adc ADC 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT 实例越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止转换、不修改数据，不分配内存。
 */
uint32_t Platform_ADC_GetLastError(platform_adc_instance_t adc);

/**
 * @brief 注册 ADC 回调。
 * @param adc ADC 实例；必须是当前板级启用并映射到 HAL 句柄的枚举值。
 * @param callbacks 待复制的回调集合；不得为 NULL，callback 成员可为 NULL 以禁用通知。
 * @retval PLATFORM_STATUS_OK 注册成功。
 * @retval PLATFORM_STATUS_BUSY 该实例已注册回调。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或回调集合非法。
 * @pre callbacks 指向的对象在调用期间有效；每个实例仅允许成功注册一次。
 * @post 成功时复制 callback 和 context，后续完成、半完成或错误事件调用该回调。
 * @warning 回调在中断上下文执行；事件对象只在回调期间有效，禁止阻塞或分配内存。
 * @note 应在初始化阶段、启动 IT/DMA 转换前注册；context 的生命周期由调用方维护。
 */
platform_status_t Platform_ADC_RegisterCallback(
  platform_adc_instance_t adc,
  const platform_adc_callback_binding_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_H
