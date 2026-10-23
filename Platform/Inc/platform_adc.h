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
typedef enum {
#if PLATFORM_ENABLE_ADC1
  PLATFORM_ADC_1 = 0U,
#endif
#if PLATFORM_ENABLE_ADC2
  PLATFORM_ADC_2 = 1U,
#endif
#if PLATFORM_ENABLE_ADC3
  PLATFORM_ADC_3 = 2U,
#endif
} Platform_ADC_Instance;

/** @brief ADC 身份槽位数量上限。 */
#define PLATFORM_ADC_INSTANCE_COUNT 3U

/** @brief ADC 错误标志，可同时置位多个位。 */
typedef enum {
  PLATFORM_ADC_ERROR_NONE     = 0u,
  PLATFORM_ADC_ERROR_INTERNAL = (1u << 0),
  PLATFORM_ADC_ERROR_OVERRUN  = (1u << 1),
  PLATFORM_ADC_ERROR_DMA      = (1u << 2),
  PLATFORM_ADC_ERROR_UNKNOWN  = (1u << 7),
} Platform_ADC_Error;

/** @brief ADC 异步事件。 */
typedef enum {
  PLATFORM_ADC_EVENT_CONV_CPLT = 0U,
  PLATFORM_ADC_EVENT_CONV_HALF,
  PLATFORM_ADC_EVENT_ERROR,
} Platform_ADC_EventType;

/** @brief ADC 事件数据，只在回调期间有效。 */
typedef struct {
  Platform_ADC_EventType type;
  uint32_t error;
} Platform_ADC_Event;

typedef void (*Platform_ADC_Callback)(
  Platform_ADC_Instance adc,
  const Platform_ADC_Event *event,
  void *context
);

/**
 * @brief ADC 回调集合。
 * @note 回调在中断上下文执行。
 * @note 一个物理实例只能有一个回调所有者。
 */
typedef struct {
  Platform_ADC_Callback callback;
  void *context;
} platform_adc_callbacks_t;

/**
 * @brief 以轮询模式启动转换。
 * @param adc ADC 实例。
 * @pre MX_ADCx_Init() 已完成，且规则组通道已配置。
 */
Platform_StatusTypeDef Platform_ADC_Start(Platform_ADC_Instance adc);

/**
 * @brief 停止轮询转换。
 * @param adc ADC 实例。
 */
Platform_StatusTypeDef Platform_ADC_Stop(Platform_ADC_Instance adc);

/**
 * @brief 等待当前轮询转换完成。
 * @param adc ADC 实例。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 */
Platform_StatusTypeDef Platform_ADC_PollForConversion(
  Platform_ADC_Instance adc,
  uint32_t timeout
);

/**
 * @brief 读取最近一次规则组转换结果。
 * @param adc ADC 实例。
 * @param value 转换结果输出。
 * @note 除非 CubeMX 另行配置，该值是原始 ADC 码值。
 */
Platform_StatusTypeDef Platform_ADC_GetValue(
  Platform_ADC_Instance adc,
  uint32_t *value
);

/**
 * @brief 执行一次阻塞式规则组转换序列。
 * @param adc ADC 实例。
 * @param value 最近一次原始转换结果的目标地址。
 * @param timeout 超时时间（ms）或 PLATFORM_TIMEOUT_INFINITE。
 * @pre CubeMX 应配置为单通道、单次转换的规则组。
 * @note 多通道扫描模式应使用 DMA。规则组中断只会报告扫描序列的最后一次
 *       转换结果。
 * @note 返回前会停止转换。
 */
Platform_StatusTypeDef Platform_ADC_Read(
  Platform_ADC_Instance adc,
  uint32_t *value,
  uint32_t timeout
);

/**
 * @brief 使用规则组中断启动转换。
 * @param adc ADC 实例。
 */
Platform_StatusTypeDef Platform_ADC_Start_IT(Platform_ADC_Instance adc);

/**
 * @brief 停止中断方式转换。
 * @param adc ADC 实例。
 */
Platform_StatusTypeDef Platform_ADC_Stop_IT(Platform_ADC_Instance adc);

/**
 * @brief 启动规则组 DMA 转换。
 * @param adc ADC 实例。
 * @param data uint32_t 转换结果的目标数组。
 * @param length 转换次数，不是字节数。
 * @note 在完成、错误或停止前，data 必须保持有效。
 * @note STM32F1 上只有 ADC1 具有 DMA 请求。对不支持的实例启动或停止 DMA
 *       会返回 Platform_ERROR。
 */
Platform_StatusTypeDef Platform_ADC_Start_DMA(
  Platform_ADC_Instance adc,
  uint32_t *data,
  uint32_t length
);

/**
 * @brief 停止 DMA 转换。
 * @param adc ADC 实例。
 * @note STM32F1 上只支持停止 ADC1。
 */
Platform_StatusTypeDef Platform_ADC_Stop_DMA(Platform_ADC_Instance adc);

/**
 * @brief 读取最近一次 HAL ADC 错误快照。
 * @param adc ADC 实例。
 */
uint32_t Platform_ADC_GetLastError(Platform_ADC_Instance adc);

/**
 * @brief 注册 ADC 回调。
 * @param adc ADC 实例。
 * @param callbacks 待复制的回调集合。
 * @note 应在初始化阶段、启动 IT/DMA 转换前调用一次。
 * @retval Platform_OK 注册成功。
 * @retval Platform_BUSY 该实例已注册回调。
 * @retval Platform_INVALID_ARGUMENT 实例或回调集合非法。
 */
Platform_StatusTypeDef Platform_ADC_RegisterCallbacks(
  Platform_ADC_Instance adc,
  const platform_adc_callbacks_t *callbacks
);

/** @} */

#endif //LAYERED_EMBEDDED_C_PLATFORM_ADC_H
