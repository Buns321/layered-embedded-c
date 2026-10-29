//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file app_calibration.c
 * @brief 速通任务一六面识别与加速度标定数学实现。
 * @ingroup app
 */

#include "app_calibration.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>

/**
 * @brief 校验静止检测阈值是否满足范围与次序约束。
 */
static bool app_calibration_stillthresholdsvalid(
  const app_calibration_thresholds_t *thresholds
) {
  assert(thresholds);

  return
    SYS_ISFINITE(thresholds->still_accel_min_mps2) &&
    SYS_ISFINITE(thresholds->still_accel_max_mps2) &&
    SYS_ISFINITE(thresholds->still_gyro_max_dps) &&
    thresholds->still_accel_min_mps2 > 0.0f &&
    thresholds->still_accel_max_mps2 >=
      thresholds->still_accel_min_mps2 &&
    thresholds->still_gyro_max_dps >= 0.0f;
}

/**
 * @brief 校验安装面检测阈值是否满足范围与次序约束。
 */
static bool app_calibration_facethresholdsvalid(
  const app_calibration_thresholds_t *thresholds
) {
  assert(thresholds);

  return
    SYS_ISFINITE(thresholds->face_dominant_min_mps2) &&
    SYS_ISFINITE(thresholds->face_cross_max_mps2) &&
    thresholds->face_dominant_min_mps2 > 0.0f &&
    thresholds->face_cross_max_mps2 >= 0.0f &&
    thresholds->face_cross_max_mps2 <
      thresholds->face_dominant_min_mps2;
}

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
) {
  if (
    accel_mps2 == NULL ||
    gyro_dps == NULL ||
    thresholds == NULL ||
    !app_calibration_stillthresholdsvalid(thresholds)
  ) {
    return false;
  }

  float norm_sq = 0.0f;
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    if (!SYS_ISFINITE(accel_mps2[axis]) ||
        !SYS_ISFINITE(gyro_dps[axis])) {
      return false;
    }
    norm_sq += accel_mps2[axis] * accel_mps2[axis];
  }

  float norm = sqrtf(norm_sq);
  if (
    !SYS_ISFINITE(norm) ||
    norm < thresholds->still_accel_min_mps2 ||
    norm > thresholds->still_accel_max_mps2
  ) {
    return false;
  }

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    if (
      SYS_ABS(gyro_dps[axis]) >
      thresholds->still_gyro_max_dps
    ) {
      return false;
    }
  }
  return true;
}

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
) {
  if (
    accel_mps2 == NULL ||
    thresholds == NULL ||
    face == NULL ||
    !app_calibration_facethresholdsvalid(thresholds)
  ) {
    return false;
  }

  float absolute[3];
  uint8_t dominant_axis = 0u;
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    if (!SYS_ISFINITE(accel_mps2[axis])) {
      return false;
    }
    absolute[axis] = SYS_ABS(accel_mps2[axis]);
    if (absolute[axis] > absolute[dominant_axis]) {
      dominant_axis = axis;
    }
  }

  if (absolute[dominant_axis] < thresholds->face_dominant_min_mps2) {
    return false;
  }
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    if (
      axis != dominant_axis &&
      absolute[axis] > thresholds->face_cross_max_mps2
    ) {
      return false;
    }
  }

  bool positive = accel_mps2[dominant_axis] > 0.0f;
  if (dominant_axis == 0u) {
    *face = positive ? APP_FACE_X_POSITIVE : APP_FACE_X_NEGATIVE;
  } else if (dominant_axis == 1u) {
    *face = positive ? APP_FACE_Y_POSITIVE : APP_FACE_Y_NEGATIVE;
  } else {
    *face = positive ? APP_FACE_Z_POSITIVE : APP_FACE_Z_NEGATIVE;
  }
  return true;
}

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
) {
  if (
    positive_face_mps2 == NULL ||
    negative_face_mps2 == NULL ||
    offset_mps2 == NULL ||
    scale == NULL ||
    axis >= 3u ||
    !SYS_ISFINITE(gravity_mps2) ||
    !SYS_ISFINITE(min_span_mps2) ||
    !SYS_ISFINITE(scale_min) ||
    !SYS_ISFINITE(scale_max) ||
    gravity_mps2 <= 0.0f ||
    min_span_mps2 <= 0.0f ||
    scale_min <= 0.0f ||
    scale_max < scale_min
  ) {
    return false;
  }

  float positive = positive_face_mps2[axis];
  float negative = negative_face_mps2[axis];
  float span = positive - negative;
  if (
    !SYS_ISFINITE(positive) ||
    !SYS_ISFINITE(negative) ||
    !SYS_ISFINITE(span) ||
    span < min_span_mps2
  ) {
    return false;
  }

  float candidate_offset = (positive + negative) * 0.5f;
  float candidate_scale = (2.0f * gravity_mps2) / span;
  if (
    !SYS_ISFINITE(candidate_offset) ||
    !SYS_ISFINITE(candidate_scale) ||
    candidate_scale < scale_min ||
    candidate_scale > scale_max
  ) {
    return false;
  }

  *offset_mps2 = candidate_offset;
  *scale = candidate_scale;
  return true;
}
