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
 * @param thresholds 待校验的静止门限；必须非 NULL，调试构建以 assert 检查。
 * @retval true 三个阈值均有限，加速度下限大于零，加速度上限不小于下限，角速度上限非负。
 * @retval false 任一阈值非有限，或范围、次序不满足上述约束。
 * @pre thresholds 非 NULL，且其生命周期覆盖本次调用。
 * @note 纯只读函数；仅在本编译单元使用，不分配内存，不修改门限。
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
 * @param thresholds 待校验的面识别门限；必须非 NULL，调试构建以 assert 检查。
 * @retval true 主轴下限有限且大于零，非主轴上限有限、非负且严格小于主轴下限。
 * @retval false 任一阈值非有限，或主轴/非主轴范围不满足上述次序。
 * @pre thresholds 非 NULL，且其生命周期覆盖本次调用。
 * @note 纯只读函数；仅在本编译单元使用，不分配内存，不修改门限。
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
