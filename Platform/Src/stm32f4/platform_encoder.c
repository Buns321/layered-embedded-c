/**
 * @file platform_encoder.c
 * @brief STM32F4 公共 encoder 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_encoder_backend.h"
#include "platform_hal_internal.h"
#include "stm32f4xx_hal.h"
#include "tim.h"

/**
 * @brief 把平台编码器实例编号转换为 HAL 句柄。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @return 对应的定时器 HAL 句柄；实例未启用或枚举值非法时返回 NULL。
 * @post 只执行实例映射，不启动定时器，也不读取或写入计数器。
 * @note 内部辅助函数，仅在任务或中断上下文执行只读映射，不分配动态内存。
 */
static TIM_HandleTypeDef *platform_encoder_to_hal(
  platform_encoder_instance_t encoder
) {
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: return &htim1;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: return &htim2;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: return &htim3;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: return &htim4;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
    case PLATFORM_ENCODER_TIM5: return &htim5;
#endif
    default: return NULL;
  }
}

/**
 * @brief 查询编码器实例是否映射到板级启用的定时器句柄。
 * @param encoder 编码器实例枚举值；有效值由 platform_encoder_instance_t 定义。
 * @retval true 实例已启用且映射到定时器 HAL 句柄。
 * @retval false 枚举值越界、实例未启用或缺少 HAL 句柄。
 * @post 只执行映射查询，不启动定时器、不修改计数器。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_encoder_backend_is_valid(
  platform_encoder_instance_t encoder
) {
  return platform_encoder_to_hal(encoder) != NULL;
}

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
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  return encoder_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_Encoder_Start(encoder_h, TIM_CHANNEL_ALL)
                     );
}

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
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  return encoder_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(
                       HAL_TIM_Encoder_Stop(encoder_h, TIM_CHANNEL_ALL)
                     );
}

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
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  if (encoder_h == NULL || counter == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  *counter = __HAL_TIM_GET_COUNTER(encoder_h);
  return PLATFORM_STATUS_OK;
}

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
) {
  TIM_HandleTypeDef *encoder_h = platform_encoder_to_hal(encoder);
  if (encoder_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  __HAL_TIM_SET_COUNTER(encoder_h, counter);
  return PLATFORM_STATUS_OK;
}

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
) {
  if (counter_bits == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  switch (encoder) {
#if PLATFORM_ENABLE_ENCODER_TIM1
    case PLATFORM_ENCODER_TIM1: *counter_bits = 16U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM2
    case PLATFORM_ENCODER_TIM2: *counter_bits = 32U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM3
    case PLATFORM_ENCODER_TIM3: *counter_bits = 16U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM4
    case PLATFORM_ENCODER_TIM4: *counter_bits = 16U; return PLATFORM_STATUS_OK;
#endif
#if PLATFORM_ENABLE_ENCODER_TIM5
    case PLATFORM_ENCODER_TIM5: *counter_bits = 32U; return PLATFORM_STATUS_OK;
#endif
    default: return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
}
