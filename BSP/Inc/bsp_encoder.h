/**
 * @file bsp_encoder.h
 * @brief 定时器正交编码器驱动（自动 16/32 位解码、极性反转与防回绕溢出）。
 * @details 基于 STM32 通用/高级定时器的硬件编码器接口模式，自动识别 16 位（`Period == 0xFFFF`）
 *          与 32 位（`Period == 0xFFFFFFFF` 且支持 32 位计数器）定时器，通过有符号半量程展开处理
 *          计数器回绕，并累计 64 位绝对位置计数。所有实例由调用方静态持有。
 *
 * @addtogroup BSP
 * @{
 */

#ifndef LAYERED_CAR_BSP_ENCODER_H
#define LAYERED_CAR_BSP_ENCODER_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 编码器初始化配置结构体。
 */
typedef struct bsp_encoder_cfg_s {
  TIM_HandleTypeDef
      *htim; /**< 硬件编码器定时器句柄指针（非 NULL，Period 须为 0xFFFF 或 0xFFFFFFFF）。 */
  uint32_t max_counts_per_period; /**< 单周期最大允许增量绝对值（单位：counts，0 表示不限幅；非 0
                                     时超限夹紧并置 overflow）。 */
  bool invert;                    /**< 方向极性反转标志；true 时对采样增量取反。 */
} bsp_encoder_cfg_t;

/**
 * @brief 编码器运行期状态对象。
 * @warning 须由调用方静态分配，严禁按值拷贝；同一控制周期内仅允许单个消费者调用
 * BSP_Encoder_GetDelta()。
 */
typedef struct bsp_encoder_s {
  TIM_HandleTypeDef *htim;        /**< 绑定的硬件定时器句柄指针；未初始化时为 NULL。 */
  uint32_t last_count;            /**< 上次采样时刻的硬件计数器原始值（16 位或 32 位）。 */
  int64_t position_counts;        /**< 自初始化或归零以来的累计位置（单位：counts）。 */
  uint32_t max_counts_per_period; /**< 单周期增量限幅阈值（单位：counts，0 表示不限幅）。 */
  uint8_t bit_width;              /**< 硬件计数器位宽（16 或 32）。 */
  bool invert;                    /**< 极性反转标志（由初始化配置拷贝）。 */
  bool has_base;                  /**< 基线就绪标志；false 表示尚未采集首帧基准计数。 */
  bool overflow;                  /**< 单周期增量超限锁存标志；需显式清除或 Resync/ResetZero。 */
} bsp_encoder_t;

/**
 * @brief 初始化编码器对象、识别 16/32 位宽并启动硬件编码器模式。
 * @param[out] enc 编码器运行期对象指针（不允许为 NULL）。
 * @param[in]  cfg 初始化配置指针（不允许为 NULL）。
 * @return 操作状态码。
 * @retval BSP_OK              初始化并启动成功。
 * @retval BSP_ERR_INVALID_ARG `enc`/`cfg`/`htim`/`Instance` 为 NULL 或定时器 Period 非满量程。
 * @retval BSP_ERR_HAL         底层 HAL_TIM_Encoder_Start() 启动失败。
 * @pre 须在 CubeMX 编码器模式定时器初始化完成后调用。
 * @note 仅限任务上下文调用；初始化后 `has_base` 为 false，首次调用 GetDelta 建立基线并返回 0。
 */
bsp_status_t BSP_Encoder_Init(bsp_encoder_t *enc, const bsp_encoder_cfg_t *cfg);

/**
 * @brief 读取并消费自上次调用以来的带符号增量。
 * @param[in,out] enc 编码器运行期对象指针（允许为 NULL，NULL 时返回 0）。
 * @return 本周期有符号增量（单位：counts）；首次建立基线或对象无效时返回 0。
 * @note 内部处理 16/32 位硬件计数器回绕并累加至 `position_counts`；
 *       若超出 `max_counts_per_period`（或超过 INT32_MAX）将夹紧输出并锁存 `overflow`。
 * @warning 同一控制周期内严禁重复调用，否则第二次调用会读到 0 并破坏速度估计。
 */
int32_t BSP_Encoder_GetDelta(bsp_encoder_t *enc);

/**
 * @brief 查询累计编码器位置（不访问硬件、不消费增量）。
 * @param[in] enc 编码器运行期对象指针（允许为 NULL，NULL 时返回 0）。
 * @return 累计位置计数（单位：counts）。
 * @note 仅读取软件缓存的 `position_counts`，不触发硬件寄存器访问。
 */
int64_t BSP_Encoder_GetPosition(const bsp_encoder_t *enc);

/**
 * @brief 重对速度基线到当前硬件计数器。
 * @param[in,out] enc 编码器运行期对象指针（允许为 NULL，NULL 时安全忽略）。
 * @note 保留累计 `position_counts`，将 `last_count` 同步为当前 CNT 并清除 `overflow` 标志，
 *       常用于控制中断恢复或模式切换后防止首周期速度尖峰。
 */
void BSP_Encoder_Resync(bsp_encoder_t *enc);

/**
 * @brief 清零硬件计数器、累计位置、基线与溢出标志。
 * @param[in,out] enc 编码器运行期对象指针（允许为 NULL，NULL 时安全忽略）。
 * @note 会直接清零硬件定时器 CNT 寄存器并将 `has_base` 置为 true。
 */
void BSP_Encoder_ResetZero(bsp_encoder_t *enc);

/**
 * @brief 查询是否锁存过单周期增量超限。
 * @param[in] enc 编码器运行期对象指针（允许为 NULL，NULL 时返回 false）。
 * @return 是否发生过超限。
 * @retval true  自上次清除以来发生过增量超限并已夹紧。
 * @retval false 未发生超限或对象无效。
 */
bool BSP_Encoder_HasOverflow(const bsp_encoder_t *enc);

/**
 * @brief 清除单周期增量超限锁存标志。
 * @param[in,out] enc 编码器运行期对象指针（允许为 NULL，NULL 时安全忽略）。
 */
void BSP_Encoder_ClearOverflow(bsp_encoder_t *enc);

#endif /* LAYERED_CAR_BSP_ENCODER_H */

/** @} */
