//
// 由 Buns_ 创建于 2026/10/18。
//

/**
 * @file system_sign.h
 * @brief 类型安全的符号函数与 SYS_SIGN 宏。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_SIGN_H
#define LAYERED_EMBEDDED_C_SYSTEM_SIGN_H

#include "system_math_def.h"

SYS_MATH_INLINE int Sys_SignInt(int x) { return (x > 0) - (x < 0); }
SYS_MATH_INLINE long Sys_SignLong(long x) { return (long)((x > 0) - (x < 0)); }
SYS_MATH_INLINE long long Sys_SignLLong(long long x) { return (long long)((x > 0) - (x < 0)); }

SYS_MATH_INLINE unsigned int Sys_SignUInt(unsigned int x) { return (x != 0u) ? 1u : 0u; }
SYS_MATH_INLINE unsigned long Sys_SignULong(unsigned long x) { return (x != 0ul) ? 1ul : 0ul; }
SYS_MATH_INLINE unsigned long long Sys_SignULLong(unsigned long long x) { return (x != 0ull) ? 1ull : 0ull; }

SYS_MATH_INLINE float Sys_SignF32(float x) { return (float)((x > 0.0f) - (x < 0.0f)); }
SYS_MATH_INLINE double Sys_SignF64(double x) { return (double)((x > 0.0) - (x < 0.0)); }

#define SYS_SIGN(x) _Generic((x), \
  char:               Sys_SignInt, \
  signed char:        Sys_SignInt, \
  short:              Sys_SignInt, \
  int:                Sys_SignInt, \
  long:               Sys_SignLong, \
  long long:          Sys_SignLLong, \
  unsigned char:      Sys_SignUInt, \
  unsigned short:     Sys_SignUInt, \
  unsigned int:       Sys_SignUInt, \
  unsigned long:      Sys_SignULong, \
  unsigned long long: Sys_SignULLong, \
  float:              Sys_SignF32, \
  double:             Sys_SignF64 \
)(x)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_SIGN_H
