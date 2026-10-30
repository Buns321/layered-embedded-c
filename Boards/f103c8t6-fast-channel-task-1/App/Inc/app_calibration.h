//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_calibration.h
 * @brief 速通任务一六面识别与加速度标定数学接口。
 * @addtogroup app
 * @{
 * @note 本模块不依赖 RTOS、HAL 或具体开发板，可由原生测试直接验证。
 */

#ifndef LAYERED_EMBEDDED_C_APP_CALIBRATION_H
#define LAYERED_EMBEDDED_C_APP_CALIBRATION_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 六面位掩码。
 */
typedef enum app_face_mask_e {
  APP_FACE_X_POSITIVE = 1U << 0,
  APP_FACE_X_NEGATIVE = 1U << 1,
  APP_FACE_Y_POSITIVE = 1U << 2,
  APP_FACE_Y_NEGATIVE = 1U << 3,
  APP_FACE_Z_POSITIVE = 1U << 4,
  APP_FACE_Z_NEGATIVE = 1U << 5,
  APP_FACE_ALL =
    APP_FACE_X_POSITIVE |
    APP_FACE_X_NEGATIVE |
    APP_FACE_Y_POSITIVE |
    APP_FACE_Y_NEGATIVE |
    APP_FACE_Z_POSITIVE |
    APP_FACE_Z_NEGATIVE,
} app_face_mask_t;

/**
 * @brief 静止与面识别的板级门限。
 * @note 静止加速度下限必须大于零且不大于上限；主轴门限必须大于非主轴门限。
 */
typedef struct app_calibration_thresholds_s {
  /** @brief 静止加速度模长下限，单位 m/s²。 */
  float still_accel_min_mps2;
  /** @brief 静止加速度模长上限，单位 m/s²。 */
  float still_accel_max_mps2;
  /** @brief 静止三轴角速度绝对值上限，单位 deg/s。 */
  float still_gyro_max_dps;
  /** @brief 面识别主轴绝对加速度下限，单位 m/s²。 */
  float face_dominant_min_mps2;
  /** @brief 面识别非主轴绝对加速度上限，单位 m/s²。 */
  float face_cross_max_mps2;
} app_calibration_thresholds_t;

/**
 * @brief 判断原始六轴样本是否满足静止门限。
 * @param accel_mps2 三轴加速度数组，按 X/Y/Z 排列，单位 m/s²；仅读取前 3 个元素。
 * @param gyro_dps 三轴角速度数组，按 X/Y/Z 排列，单位 deg/s；仅读取前 3 个元素。
 * @param thresholds 静止判定门限；要求加速度上下限有限且 0 < 下限 <= 上限，角速度上限有限且非负。
 * @retval true 三轴加速度模长位于 [still_accel_min_mps2, still_accel_max_mps2]，
 *             且每个角速度绝对值不超过 still_gyro_max_dps。
 * @retval false 指针为 NULL、门限不合法、任一分量非有限，或不满足上述范围。
 * @note 纯只读函数，不修改输入、不分配内存，可由任意任务或 ISR 并发调用。
 */
bool App_Calibration_IsStill(
  const float accel_mps2[3],
  const float gyro_dps[3],
  const app_calibration_thresholds_t *thresholds
);

/**
 * @brief 从加速度方向识别唯一面。
 * @param accel_mps2 三轴加速度数组，按 X/Y/Z 排列，单位 m/s²；仅读取前 3 个元素。
 * @param thresholds 面识别门限；要求主轴下限有限且大于零，非主轴上限有限、非负且小于主轴下限。
 * @param face 输出单个六面位掩码；仅在返回 true 时写入，失败时保持调用前的值不变。
 * @retval true 最大绝对分量达到主轴下限，其余分量不超过非主轴上限，并已写入唯一面。
 * @retval false 指针为 NULL、门限不合法、任一分量非有限，或样本倾斜到无法唯一识别安装面。
 * @post 成功时 *face 恰为 APP_FACE_* 中表示正负 X/Y/Z 的一个位。
 * @note 纯函数；不分配内存，不访问共享状态，可由任意任务或 ISR 调用。
 */
bool App_Calibration_DetectFace(
  const float accel_mps2[3],
  const app_calibration_thresholds_t *thresholds,
  app_face_mask_t *face
);

/**
 * @brief 由正负两面平均加速度计算单轴零偏和比例系数。
 * @param positive_face_mps2 正方向朝上时的三轴平均加速度，单位 m/s²；只读取 axis 指定的分量。
 * @param negative_face_mps2 负方向朝上时的三轴平均加速度，单位 m/s²；只读取 axis 指定的分量。
 * @param axis 传感器轴下标，有效范围 0..2（X/Y/Z）。
 * @param gravity_mps2 用于反算比例系数的标准重力，单位 m/s²；必须有限且大于零。
 * @param min_span_mps2 正负样本差值下限，单位 m/s²；必须有限且大于零。
 * @param scale_min 允许的最小无量纲比例系数，必须有限且大于零。
 * @param scale_max 允许的最大无量纲比例系数，必须有限且不小于 scale_min。
 * @param offset_mps2 输出零偏，单位 m/s²；仅在返回 true 时写入。
 * @param scale 输出无量纲比例系数；仅在返回 true 时写入。
 * @retval true 正负样本差值达到 min_span_mps2，且计算出的比例系数位于 [scale_min, scale_max]。
 * @retval false 指针为空、轴或标量参数非法、样本非有限、差值不足，或比例系数越界；输出保持不变。
 * @post 成功时 *offset_mps2 = (positive + negative) / 2，
 *       *scale = 2 * gravity_mps2 / (positive - negative)。
 * @note 纯函数；不分配内存，不修改输入数组，可由任意任务或 ISR 调用。
 */
bool App_Calibration_ComputeAccelAxis(
  const float positive_face_mps2[3],
  const float negative_face_mps2[3],
  uint8_t axis,
  float gravity_mps2,
  float min_span_mps2,
  float scale_min,
  float scale_max,
  float *offset_mps2,
  float *scale
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_CALIBRATION_H
