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

SYS_MATH_INLINE bool Sys_IsNan_F32(float x) {
  Sys_Math_F32_Bits v;
  v.f = x;
  return ((v.u & 0x7F800000u) == 0x7F800000u) &&
         ((v.u & 0x007FFFFFu) != 0u);
}

SYS_MATH_INLINE bool Sys_IsInf_F32(float x) {
  Sys_Math_F32_Bits v;
  v.f = x;
  return (v.u & 0x7FFFFFFFu) == 0x7F800000u;
}

SYS_MATH_INLINE bool Sys_IsFinite_F32(float x) {
  Sys_Math_F32_Bits v;
  v.f = x;
  return (v.u & 0x7F800000u) != 0x7F800000u;
}

SYS_MATH_INLINE bool Sys_IsNan_F64(double x) {
  Sys_Math_F64_Bits v;
  v.f = x;
  return ((v.u & 0x7FF0000000000000ull) == 0x7FF0000000000000ull) &&
         ((v.u & 0x000FFFFFFFFFFFFFull) != 0ull);
}

SYS_MATH_INLINE bool Sys_IsInf_F64(double x) {
  Sys_Math_F64_Bits v;
  v.f = x;
  return (v.u & 0x7FFFFFFFFFFFFFFFull) == 0x7FF0000000000000ull;
}

SYS_MATH_INLINE bool Sys_IsFinite_F64(double x) {
  Sys_Math_F64_Bits v;
  v.f = x;
  return (v.u & 0x7FF0000000000000ull) != 0x7FF0000000000000ull;
}

#define SYS_ISNAN(x) _Generic((x), \
  float:  Sys_IsNan_F32, \
  double: Sys_IsNan_F64 \
)(x)

#define SYS_ISINF(x) _Generic((x), \
  float:  Sys_IsInf_F32, \
  double: Sys_IsInf_F64 \
)(x)

#define SYS_ISFINITE(x) _Generic((x), \
  float:  Sys_IsFinite_F32, \
  double: Sys_IsFinite_F64 \
)(x)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_CHECK_H
