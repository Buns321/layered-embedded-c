/**
 * @file bsp_encoder.h
 * @brief 定时器正交编码器驱动（自动 16/32 位解码、极性反转与防回绕溢出）。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_ENCODER_H
#define LAYERED_CAR_BSP_ENCODER_H

#include "bsp_def.h"
#include "bsp_mcu.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 编码器初始化配置。
 */
typedef struct bsp_encoder_cfg_s {
  TIM_HandleTypeDef *htim;
  uint32_t max_counts_per_period; /* 0 = 不限幅；非 0 时夹紧增量并置 overflow */
  bool invert;                    /* true 时对采样增量取反 */
} bsp_encoder_cfg_t;

/**
 * @brief 编码器运行期状态对象；须静态持有且同一控制周期仅允许单消费者读增量。
 */
typedef struct bsp_encoder_s {
  TIM_HandleTypeDef *htim;
  uint32_t last_count;
  int64_t position_counts;
  uint32_t max_counts_per_period;
  uint8_t bit_width; /* 16 或 32 */
  bool invert;
  bool has_base;
  bool overflow;
} bsp_encoder_t;

/**
 * @brief 初始化编码器对象、识别 16/32 位宽并启动硬件编码器模式。
 */
bsp_status_t BSP_Encoder_Init(bsp_encoder_t *enc, const bsp_encoder_cfg_t *cfg);

/**
 * @brief 读取并消费自上次调用以来的带符号增量，单位 counts；首次调用建立基线返回 0。
 */
int32_t BSP_Encoder_GetDelta(bsp_encoder_t *enc);

/**
 * @brief 查询累计编码器位置，单位 counts（不访问硬件、不消费增量）。
 */
int64_t BSP_Encoder_GetPosition(const bsp_encoder_t *enc);

/**
 * @brief 重对速度基线到当前硬件计数器，保留 position_counts 并清除 overflow。
 */
void BSP_Encoder_Resync(bsp_encoder_t *enc);

/**
 * @brief 清零硬件计数器、position_counts、基线与 overflow 标志。
 */
void BSP_Encoder_ResetZero(bsp_encoder_t *enc);

/**
 * @brief 查询是否锁存过单周期增量超限。
 */
bool BSP_Encoder_HasOverflow(const bsp_encoder_t *enc);

/**
 * @brief 清除 overflow 锁存标志。
 */
void BSP_Encoder_ClearOverflow(bsp_encoder_t *enc);

/** @} */

#endif /* LAYERED_CAR_BSP_ENCODER_H */
