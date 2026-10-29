//
// 由 Buns_ 创建于 2026/10/18。
//

/**
 * @file system_check.h
 * @brief 浮点 NaN / Inf / 有限性检查。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_CHECK_H
#define LAYERED_EMBEDDED_C_SYSTEM_CHECK_H

#include "system_math_def.h"
#include <stdbool.h>

/**
 * @brief 判断单精度浮点数是否为 NaN。
 * @param x 输入值。
 * @return x 为 NaN 时返回 true。
 */
SYS_MATH_INLINE bool Sys_IsNanF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return ((v.u & 0x7F800000u) == 0x7F800000u) &&
         ((v.u & 0x007FFFFFu) != 0u);
}

/**
 * @brief 判断单精度浮点数是否为正负无穷。
 * @param x 输入值。
 * @return x 为无穷时返回 true。
 */
SYS_MATH_INLINE bool Sys_IsInfF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFu) == 0x7F800000u;
}

/**
 * @brief 判断单精度浮点数是否为有限值。
 * @param x 输入值。
 * @return x 既不是 NaN 也不是无穷时返回 true。
 */
SYS_MATH_INLINE bool Sys_IsFiniteF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7F800000u) != 0x7F800000u;
}

/**
 * @brief 判断双精度浮点数是否为 NaN。
 * @param x 输入值。
 * @return x 为 NaN 时返回 true。
 */
SYS_MATH_INLINE bool Sys_IsNanF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return ((v.u & 0x7FF0000000000000ull) == 0x7FF0000000000000ull) &&
         ((v.u & 0x000FFFFFFFFFFFFFull) != 0ull);
}

/**
 * @brief 判断双精度浮点数是否为正负无穷。
 * @param x 输入值。
 * @return x 为无穷时返回 true。
 */
SYS_MATH_INLINE bool Sys_IsInfF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull;
}

/**
 * @brief 判断双精度浮点数是否为有限值。
 * @param x 输入值。
 * @return x 既不是 NaN 也不是无穷时返回 true。
 */
SYS_MATH_INLINE bool Sys_IsFiniteF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

/**
 * @brief 判断浮点数是否为 NaN。
 * @param x 单精度或双精度浮点值；其类型必须为 float 或 double。
 * @return x 为 NaN 时返回 true。
 * @note `_Generic` 控制表达式不求值，x 作为实参只求值一次。
 */
#define SYS_ISNAN(x) _Generic((x), \
  float:  Sys_IsNanF32, \
  double: Sys_IsNanF64 \
)(x)

/**
 * @brief 判断浮点数是否为正无穷或负无穷。
 * @param x 单精度或双精度浮点值；其类型必须为 float 或 double。
 * @return x 为正负无穷时返回 true。
 * @note `_Generic` 控制表达式不求值，x 作为实参只求值一次。
 */
#define SYS_ISINF(x) _Generic((x), \
  float:  Sys_IsInfF32, \
  double: Sys_IsInfF64 \
)(x)

/**
 * @brief 判断浮点数是否为有限值。
 * @param x 单精度或双精度浮点值；其类型必须为 float 或 double。
 * @return x 既不是 NaN 也不是无穷时返回 true。
 * @note `_Generic` 控制表达式不求值，x 作为实参只求值一次。
 */
#define SYS_ISFINITE(x) _Generic((x), \
  float:  Sys_IsFiniteF32, \
  double: Sys_IsFiniteF64 \
)(x)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_CHECK_H
