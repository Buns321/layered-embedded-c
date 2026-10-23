//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_base.h
 * @brief 编码器抽象：位置、增量与基线重同步。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_ENCODER_BASE_H
#define LAYERED_EMBEDDED_C_ENCODER_BASE_H

#include <stdint.h>

typedef struct encoder_base_s encoder_base_t;

/**
 * @brief 编码器具体实现的操作表。
 */
typedef struct encoder_ops_s {
  void    (*set_zero)     (encoder_base_t *encoder_h); // 设当前位置为零点
  int64_t (*get_position) (encoder_base_t *encoder_h); // 获取累计位置                   (count)
  int32_t (*get_delta)    (encoder_base_t *encoder_h); // 获取自上次采样的编码器的变化值 (count)
  void    (*resync)       (encoder_base_t *encoder_h);
} encoder_ops_t;

/**
 * @brief 编码器抽象对象。
 */
typedef struct encoder_base_s {
  const encoder_ops_t *ops;
  const char *name;
} encoder_base_t;

/** @brief 初始化编码器抽象层并绑定操作表。 */
void Dev_Encoder_Base_Init(encoder_base_t *encoder_h, const char *name, const encoder_ops_t *ops);
/**
 * @brief 把当前位置设为零点。
 * @note 会改变累计位置。
 */
void Dev_Encoder_Base_SetZero(encoder_base_t *encoder_h);
/** @brief 读取累计位置（counts）。 */
int64_t Dev_Encoder_Base_GetPosition(encoder_base_t *encoder_h);
/**
 * @brief 读取自上次调用以来的增量（counts）。
 * @warning 该接口会消费增量；每个控制周期只调用一次。
 */
int32_t Dev_Encoder_Base_GetDelta(encoder_base_t *encoder_h);
/**
 * @brief 重新对齐速度基线，不改变累计位置。
 */
void Dev_Encoder_Base_Resync(encoder_base_t *encoder_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_ENCODER_BASE_H
