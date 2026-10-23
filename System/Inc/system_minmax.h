//
// 由 Buns_ 创建于 2026/10/18。
//

/**
 * @file system_minmax.h
 * @brief 类型安全的最小/最大值与 SYS_MIN / SYS_MAX 宏。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_MINMAX_H
#define LAYERED_EMBEDDED_C_SYSTEM_MINMAX_H

#include "system_math_def.h"

SYS_MATH_INLINE int Sys_MinInt(int a, int b) { return (a < b) ? a : b; }
SYS_MATH_INLINE int Sys_MaxInt(int a, int b) { return (a > b) ? a : b; }

SYS_MATH_INLINE long Sys_MinLong(long a, long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE long Sys_MaxLong(long a, long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE long long Sys_MinLLong(long long a, long long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE long long Sys_MaxLLong(long long a, long long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned int Sys_MinUInt(unsigned int a, unsigned int b) { return (a < b) ? a : b; }
SYS_MATH_INLINE unsigned int Sys_MaxUInt(unsigned int a, unsigned int b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned long Sys_MinULong(unsigned long a, unsigned long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE unsigned long Sys_MaxULong(unsigned long a, unsigned long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned long long Sys_MinULLong(unsigned long long a, unsigned long long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE unsigned long long Sys_MaxULLong(unsigned long long a, unsigned long long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE float Sys_MinF32(float a, float b) { return (a < b) ? a : b; }
SYS_MATH_INLINE float Sys_MaxF32(float a, float b) { return (a > b) ? a : b; }

SYS_MATH_INLINE double Sys_MinF64(double a, double b) { return (a < b) ? a : b; }
SYS_MATH_INLINE double Sys_MaxF64(double a, double b) { return (a > b) ? a : b; }

#define SYS_MIN(a, b) _Generic((a), \
  char:               Sys_MinInt, \
  signed char:        Sys_MinInt, \
  short:              Sys_MinInt, \
  int:                Sys_MinInt, \
  long:               Sys_MinLong, \
  long long:          Sys_MinLLong, \
  unsigned char:      Sys_MinUInt, \
  unsigned short:     Sys_MinUInt, \
  unsigned int:       Sys_MinUInt, \
  unsigned long:      Sys_MinULong, \
  unsigned long long: Sys_MinULLong, \
  float:              Sys_MinF32, \
  double:             Sys_MinF64 \
)(a, b)

#define SYS_MAX(a, b) _Generic((a), \
  char:               Sys_MaxInt, \
  signed char:        Sys_MaxInt, \
  short:              Sys_MaxInt, \
  int:                Sys_MaxInt, \
  long:               Sys_MaxLong, \
  long long:          Sys_MaxLLong, \
  unsigned char:      Sys_MaxUInt, \
  unsigned short:     Sys_MaxUInt, \
  unsigned int:       Sys_MaxUInt, \
  unsigned long:      Sys_MaxULong, \
  unsigned long long: Sys_MaxULLong, \
  float:              Sys_MaxF32, \
  double:             Sys_MaxF64 \
)(a, b)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_MINMAX_H
