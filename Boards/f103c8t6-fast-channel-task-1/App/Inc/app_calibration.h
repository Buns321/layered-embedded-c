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
 * @param accel_mps2 三轴加速度（m/s²）。
 * @param gyro_dps 三轴角速度（deg/s）。
 * @param thresholds 门限。
 * @return 门限有效、所有输入有限且满足门限时返回 true。
 */
bool App_Calibration_IsStill(
  const float accel_mps2[3],
  const float gyro_dps[3],
  const app_calibration_thresholds_t *thresholds
);

/**
 * @brief 从加速度方向识别唯一面。
 * @param accel_mps2 三轴加速度（m/s²）。
 * @param thresholds 主轴和非主轴门限。
 * @param face 输出六面位掩码。
 * @retval true 识别成功；门限要求主轴门限大于非主轴门限。
 * @retval false 门限无效、输入非有限、倾斜过大或没有主轴。
 */
bool App_Calibration_DetectFace(
  const float accel_mps2[3],
  const app_calibration_thresholds_t *thresholds,
  app_face_mask_t *face
);

/**
 * @brief 由正负两面平均加速度计算单轴零偏和比例系数。
 * @param positive_face_mps2 正方向朝上时三轴平均加速度。
 * @param negative_face_mps2 负方向朝上时三轴平均加速度。
 * @param axis 轴下标 0..2。
 * @param gravity_mps2 标准重力。
 * @param min_span_mps2 正负样本最小差值。
 * @param scale_min 比例系数下限。
 * @param scale_max 比例系数上限。
 * @param offset_mps2 输出零偏。
 * @param scale 输出比例系数。
 * @retval true 公式结果有效。
 * @retval false 参数或正负样本无效。
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
