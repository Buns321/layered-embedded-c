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

SYS_MATH_INLINE int Sys_Sign_Int(int x) { return (x > 0) - (x < 0); }
SYS_MATH_INLINE long Sys_Sign_Long(long x) { return (long)((x > 0) - (x < 0)); }
SYS_MATH_INLINE long long Sys_Sign_LLong(long long x) { return (long long)((x > 0) - (x < 0)); }

SYS_MATH_INLINE unsigned int Sys_Sign_UInt(unsigned int x) { return (x != 0u) ? 1u : 0u; }
SYS_MATH_INLINE unsigned long Sys_Sign_ULong(unsigned long x) { return (x != 0ul) ? 1ul : 0ul; }
SYS_MATH_INLINE unsigned long long Sys_Sign_ULLong(unsigned long long x) { return (x != 0ull) ? 1ull : 0ull; }

SYS_MATH_INLINE float Sys_Sign_F32(float x) { return (float)((x > 0.0f) - (x < 0.0f)); }
SYS_MATH_INLINE double Sys_Sign_F64(double x) { return (double)((x > 0.0) - (x < 0.0)); }

#define SYS_SIGN(x) _Generic((x), \
  char:               Sys_Sign_Int, \
  signed char:        Sys_Sign_Int, \
  short:              Sys_Sign_Int, \
  int:                Sys_Sign_Int, \
  long:               Sys_Sign_Long, \
  long long:          Sys_Sign_LLong, \
  unsigned char:      Sys_Sign_UInt, \
  unsigned short:     Sys_Sign_UInt, \
  unsigned int:       Sys_Sign_UInt, \
  unsigned long:      Sys_Sign_ULong, \
  unsigned long long: Sys_Sign_ULLong, \
  float:              Sys_Sign_F32, \
  double:             Sys_Sign_F64 \
)(x)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_SIGN_H
