/**
 * @file platform_encoder_backend.h
 * @brief STM32 系列公共 encoder 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H

#include "platform_def.h"
#include "platform_encoder.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 查询编码器实例是否映射到板级启用的定时器句柄。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @retval true 实例已启用且映射到定时器 HAL 句柄。
 * @retval false 枚举值越界、实例未启用或缺少 HAL 句柄。
 * @post 只执行映射查询，不启动定时器、不修改计数器。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_encoder_backend_is_valid(platform_encoder_instance_t encoder);

/**
 * @brief 启动定时器的两个编码器输入通道。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @retval PLATFORM_STATUS_OK 编码器接口已启动。
 * @retval PLATFORM_STATUS_BUSY HAL 报告定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre MX_TIMx_Init() 已完成，且定时器由 CubeMX 配置为 Encoder 模式。
 * @post 两个编码器通道同时启动；本函数不选择 TI1、TI2 或 TI12 模式。
 * @note 不执行 CubeMX 结构性初始化，也不分配动态内存；编码器模式下无 PWM 启动语义。
 */
platform_status_t platform_encoder_backend_start(
  platform_encoder_instance_t encoder
);

/**
 * @brief 停止定时器的两个编码器输入通道。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @retval PLATFORM_STATUS_OK 编码器接口已停止。
 * @retval PLATFORM_STATUS_BUSY HAL 报告定时器忙。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @post 停止后计数器不再随编码器输入变化；当前计数值保持到下次读取或写入。
 * @note 不关闭定时器时钟，也不重置计数器；不分配动态内存。
 */
platform_status_t platform_encoder_backend_stop(
  platform_encoder_instance_t encoder
);

/**
 * @brief 读取编码器硬件计数器的当前原始值。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @param counter 输出指针，接收 counts（原始计数器值）；调用方拥有存储，非 NULL。
 * @retval PLATFORM_STATUS_OK 已将当前计数写入 *counter。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法、未启用、无 HAL 句柄或 counter 为 NULL。
 * @pre counter 指向可写存储；当前计数值范围为 0..(2^counter_bits - 1)。
 * @post 不停止或清零计数器，也不计算自上次读取以来的增量；调用方自行做回绕和增量处理。
 * @note 读取是相对硬件计数的快照，任务与中断并发读取时由调用方处理一致性。
 */
platform_status_t platform_encoder_backend_get_counter(
  platform_encoder_instance_t encoder,
  uint32_t *counter
);

/**
 * @brief 写入编码器硬件计数器。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @param counter 目标 counts 值；必须落在该定时器计数位宽对应的 0..(2^counter_bits - 1) 范围。
 * @retval PLATFORM_STATUS_OK 计数器已写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre 调用方已通过 GetCounterBits 确认位宽，且没有依赖本次写入的并发计数操作。
 * @post 硬件计数器立即变为 counter；本函数不停止编码器，也不改变输入捕获配置。
 * @note 本层不检查 counter 是否超出定时器位宽；超范围值的截断行为由硬件决定。
 */
platform_status_t platform_encoder_backend_set_counter(
  platform_encoder_instance_t encoder,
  uint32_t counter
);

/**
 * @brief 查询编码器定时器的计数器位宽。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @param counter_bits 输出指针，接收 16 或 32（bit）；调用方拥有存储，非 NULL。
 * @retval PLATFORM_STATUS_OK 位宽已写入 *counter_bits。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法、未启用或 counter_bits 为 NULL。
 * @pre counter_bits 指向可写的 uint8_t。
 * @post 成功时 *counter_bits 为对应定时器的硬件位宽，可用于计数回绕和写入范围检查。
 * @note 位宽由 MCU 定时器实例决定；不访问计数器寄存器，也不修改运行状态。
 */
platform_status_t platform_encoder_backend_get_counter_bits(
  platform_encoder_instance_t encoder,
  uint8_t *counter_bits
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_ENCODER_BACKEND_H
