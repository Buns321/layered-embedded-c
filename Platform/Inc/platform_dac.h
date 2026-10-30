//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file platform_dac.h
 * @brief 不依赖 HAL 类型的 DAC 输出接口。
 * @addtogroup platform
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_DAC_H
#define LAYERED_EMBEDDED_C_PLATFORM_DAC_H

#include "platform_config.h"
#include "platform_def.h"
#include <stdint.h>

#if !PLATFORM_ENABLE_DAC1
#error "Platform DAC1 must be enabled before including platform_dac.h"
#endif

/** @brief 可用的 DAC 实例。 */
typedef enum platform_dac_instance_e {
  PLATFORM_DAC_1 = 0U,
} platform_dac_instance_t;

/** @brief DAC 身份槽位数量上限。 */
#define PLATFORM_DAC_INSTANCE_COUNT 1U

/** @brief DAC 输出通道。 */
typedef enum platform_dac_channel_e {
  PLATFORM_DAC_CHANNEL_1 = 0U,
  PLATFORM_DAC_CHANNEL_2 = 1U,
} platform_dac_channel_t;

/** @brief 已配置 DAC 通道使用的数值对齐方式。 */
typedef enum platform_dac_alignment_e {
  PLATFORM_DAC_ALIGN_8_BIT_RIGHT = 0U,
  PLATFORM_DAC_ALIGN_12_BIT_RIGHT,
  PLATFORM_DAC_ALIGN_12_BIT_LEFT,
} platform_dac_alignment_t;

/** @brief DAC 错误标志。 */
typedef enum platform_dac_error_e {
  PLATFORM_DAC_ERROR_NONE          = 0u,
  PLATFORM_DAC_ERROR_DMA_UNDERRUN  = (1u << 0),
  PLATFORM_DAC_ERROR_DMA           = (1u << 1),
  PLATFORM_DAC_ERROR_TIMEOUT       = (1u << 2),
  PLATFORM_DAC_ERROR_UNKNOWN       = (1u << 7),
} platform_dac_error_t;

/** @brief DAC 异步事件。 */
typedef enum platform_dac_event_type_e {
  PLATFORM_DAC_EVENT_CONV_COMPLETE = 0U,
  PLATFORM_DAC_EVENT_CONV_HALF_COMPLETE,
  PLATFORM_DAC_EVENT_ERROR,
  PLATFORM_DAC_EVENT_UNDERRUN,
} platform_dac_event_type_t;

/** @brief DAC 事件数据，只在回调期间有效。 */
typedef struct platform_dac_event_s {
  platform_dac_event_type_t type;
  platform_dac_channel_t channel;
  uint32_t error;
} platform_dac_event_t;

typedef void (*platform_dac_callback_t)(
  platform_dac_instance_t dac,
  const platform_dac_event_t *event,
  void *context
);

/**
 * @brief DAC 转换回调集合。
 * @note 回调在中断上下文执行。
 * @note 一个物理 DAC 实例只能有一个回调所有者。
 */
typedef struct platform_dac_callback_binding_s {
  platform_dac_callback_t callback;
  void *context;
} platform_dac_callback_binding_t;

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
);

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
);

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
);

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
);

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
);

/**
 * @brief 读取最近一次 HAL DAC 错误快照。
 * @param dac DAC 实例；当前仅有效枚举值可读取后端错误。
 * @retval PLATFORM_ERROR_INVALID_ARGUMENT dac 越界。
 * @post 合法实例返回软件错误位与 HAL 错误映射的按位或；非法实例不访问快照。
 * @note 只读，不清除软件或 HAL 错误；不停止通道、不修改输出数据，不分配内存。
 */
uint32_t Platform_DAC_GetLastError(platform_dac_instance_t dac);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_DAC_H
