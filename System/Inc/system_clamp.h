//
// Created by Buns_ on 2026/10/18.
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

SYS_MATH_INLINE int Sys_Clamp_Int(int x, int lo, int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE long Sys_Clamp_Long(long x, long lo, long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE long long Sys_Clamp_LLong(long long x, long long lo, long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE unsigned int Sys_Clamp_UInt(unsigned int x, unsigned int lo, unsigned int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE unsigned long Sys_Clamp_ULong(unsigned long x, unsigned long lo, unsigned long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE unsigned long long Sys_Clamp_ULLong(unsigned long long x, unsigned long long lo, unsigned long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE float Sys_Clamp_F32(float x, float lo, float hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

SYS_MATH_INLINE double Sys_Clamp_F64(double x, double lo, double hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

#define SYS_CLAMP(x, lo, hi) _Generic((x), \
  char:               Sys_Clamp_Int, \
  signed char:        Sys_Clamp_Int, \
  short:              Sys_Clamp_Int, \
  int:                Sys_Clamp_Int, \
  long:               Sys_Clamp_Long, \
  long long:          Sys_Clamp_LLong, \
  unsigned char:      Sys_Clamp_UInt, \
  unsigned short:     Sys_Clamp_UInt, \
  unsigned int:       Sys_Clamp_UInt, \
  unsigned long:      Sys_Clamp_ULong, \
  unsigned long long: Sys_Clamp_ULLong, \
  float:              Sys_Clamp_F32, \
  double:             Sys_Clamp_F64 \
)(x, lo, hi)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H
