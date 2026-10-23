//
// 由 Buns_ 创建于 2026/8/16。
//

/**
 * @file system_abs.h
 * @brief 类型安全的绝对值，含整数饱和版本。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_ABS_H
#define LAYERED_EMBEDDED_C_SYSTEM_ABS_H

#include "system_math_def.h"
#include <limits.h>

/* 有符号整数：条件取反，Cortex-M 上编译为 IT + RSB，无跳转 */
SYS_MATH_INLINE int Sys_Abs_Int(int x) {
  return (x < 0) ? -x : x;
}

SYS_MATH_INLINE long Sys_Abs_Long(long x) {
  return (x < 0) ? -x : x;
}

SYS_MATH_INLINE long long Sys_Abs_LLong(long long x) {
  return (x < 0) ? -x : x;
}

/* 无符号整数：原样返回 */
SYS_MATH_INLINE unsigned int Sys_Abs_UInt(unsigned int x) { return x; }
SYS_MATH_INLINE unsigned long Sys_Abs_ULong(unsigned long x) { return x; }
SYS_MATH_INLINE unsigned long long Sys_Abs_ULLong(unsigned long long x) { return x; }

/* 浮点：清符号位，不依赖 FPU */
SYS_MATH_INLINE float Sys_Abs_F32(float x) {
  Sys_Math_F32_Bits v;
  v.f = x;
  v.u &= 0x7FFFFFFFu;
  return v.f;
}

SYS_MATH_INLINE double Sys_Abs_F64(double x) {
  Sys_Math_F64_Bits v;
  v.f = x;
  v.u &= 0x7FFFFFFFFFFFFFFFull;
  return v.f;
}

/* 饱和版本：最小值取绝对值后返回最大值，避免溢出 */
SYS_MATH_INLINE int Sys_Abs_Int_Sat(int x) {
  return (x == INT_MIN) ? INT_MAX : ((x < 0) ? -x : x);
}

SYS_MATH_INLINE long Sys_Abs_Long_Sat(long x) {
  return (x == LONG_MIN) ? LONG_MAX : ((x < 0) ? -x : x);
}

SYS_MATH_INLINE long long Sys_Abs_LLong_Sat(long long x) {
  return (x == LLONG_MIN) ? LLONG_MAX : ((x < 0) ? -x : x);
}

/* 通用入口：C11 _Generic，表达式只求值一次 */
#define SYS_ABS(x) _Generic((x), \
  char:               Sys_Abs_Int_Sat, \
  signed char:        Sys_Abs_Int_Sat, \
  short:              Sys_Abs_Int_Sat, \
  int:                Sys_Abs_Int_Sat, \
  long:               Sys_Abs_Long_Sat, \
  long long:          Sys_Abs_LLong_Sat, \
  unsigned char:      Sys_Abs_UInt, \
  unsigned short:     Sys_Abs_UInt, \
  unsigned int:       Sys_Abs_UInt, \
  unsigned long:      Sys_Abs_ULong, \
  unsigned long long: Sys_Abs_ULLong, \
  float:              Sys_Abs_F32, \
  double:             Sys_Abs_F64 \
)(x)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_ABS_H
