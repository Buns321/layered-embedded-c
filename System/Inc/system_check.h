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
 * @param x 待检查的 float 值；按 IEEE 754 位模式解释，不触发浮点比较异常。
 * @retval true x 的指数位全 1 且尾数非 0，表示 NaN。
 * @retval false x 不是 NaN，包括有限值和正负无穷。
 * @note 纯函数，不修改 x 或共享状态；不分配动态内存，可在任务或 ISR 中调用。
 */
SYS_MATH_INLINE bool Sys_IsNanF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return ((v.u & 0x7F800000u) == 0x7F800000u) &&
         ((v.u & 0x007FFFFFu) != 0u);
}

/**
 * @brief 判断单精度浮点数是否为正负无穷。
 * @param x 待检查的 float 值；仅检查指数和尾数位，不执行浮点运算。
 * @retval true x 的指数位全 1 且尾数为 0，表示正无穷或负无穷。
 * @retval false x 是有限值或 NaN。
 * @note 纯函数，不修改 x 或共享状态；不分配动态内存，可在任务或 ISR 中调用。
 */
SYS_MATH_INLINE bool Sys_IsInfF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFu) == 0x7F800000u;
}

/**
 * @brief 判断单精度浮点数是否为有限值。
 * @param x 待检查的 float 值；按 IEEE 754 位模式解释。
 * @retval true x 的指数位不全为 1，表示普通有限值或带符号零。
 * @retval false x 是 NaN 或正负无穷。
 * @note 纯函数，不修改 x 或共享状态；不分配动态内存，可在任务或 ISR 中调用。
 */
SYS_MATH_INLINE bool Sys_IsFiniteF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7F800000u) != 0x7F800000u;
}

/**
 * @brief 判断双精度浮点数是否为 NaN。
 * @param x 待检查的 double 值；按 IEEE 754 位模式解释，不触发浮点比较异常。
 * @retval true x 的指数位全 1 且尾数非 0，表示 NaN。
 * @retval false x 不是 NaN，包括有限值和正负无穷。
 * @note 纯函数，不修改 x 或共享状态；不分配动态内存，可在任务或 ISR 中调用。
 */
SYS_MATH_INLINE bool Sys_IsNanF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return ((v.u & 0x7FF0000000000000ull) == 0x7FF0000000000000ull) &&
         ((v.u & 0x000FFFFFFFFFFFFFull) != 0ull);
}

/**
 * @brief 判断双精度浮点数是否为正负无穷。
 * @param x 待检查的 double 值；仅检查指数和尾数位，不执行浮点运算。
 * @retval true x 的指数位全 1 且尾数为 0，表示正无穷或负无穷。
 * @retval false x 是有限值或 NaN。
 * @note 纯函数，不修改 x 或共享状态；不分配动态内存，可在任务或 ISR 中调用。
 */
SYS_MATH_INLINE bool Sys_IsInfF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull;
}

/**
 * @brief 判断双精度浮点数是否为有限值。
 * @param x 待检查的 double 值；按 IEEE 754 位模式解释。
 * @retval true x 的指数位不全为 1，表示普通有限值或带符号零。
 * @retval false x 是 NaN 或正负无穷。
 * @note 纯函数，不修改 x 或共享状态；不分配动态内存，可在任务或 ISR 中调用。
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
