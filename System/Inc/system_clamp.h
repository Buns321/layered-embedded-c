//
// 由 Buns_ 创建于 2026/10/18。
//

/**
 * @file system_clamp.h
 * @brief 类型安全的限幅函数与 SYS_CLAMP 宏。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H
#define LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H

#include "system_math_def.h"
#include <assert.h>

SYS_MATH_INLINE int Sys_ClampInt(int x, int lo, int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE long Sys_ClampLong(long x, long lo, long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE long long Sys_ClampLLong(long long x, long long lo, long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE unsigned int Sys_ClampUInt(unsigned int x, unsigned int lo, unsigned int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE unsigned long Sys_ClampULong(unsigned long x, unsigned long lo, unsigned long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE unsigned long long Sys_ClampULLong(unsigned long long x, unsigned long long lo, unsigned long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE float Sys_ClampF32(float x, float lo, float hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE double Sys_ClampF64(double x, double lo, double hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

#define SYS_CLAMP(x, lo, hi) _Generic((x), \
  char:               Sys_ClampInt, \
  signed char:        Sys_ClampInt, \
  short:              Sys_ClampInt, \
  int:                Sys_ClampInt, \
  long:               Sys_ClampLong, \
  long long:          Sys_ClampLLong, \
  unsigned char:      Sys_ClampUInt, \
  unsigned short:     Sys_ClampUInt, \
  unsigned int:       Sys_ClampUInt, \
  unsigned long:      Sys_ClampULong, \
  unsigned long long: Sys_ClampULLong, \
  float:              Sys_ClampF32, \
  double:             Sys_ClampF64 \
)(x, lo, hi)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H
