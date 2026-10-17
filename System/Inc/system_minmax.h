//
// Created by Buns_ on 2026/10/18.
//

#ifndef LAYERED_EMBEDDED_C_SYSTEM_MINMAX_H
#define LAYERED_EMBEDDED_C_SYSTEM_MINMAX_H

#include "system_math_def.h"

SYS_MATH_INLINE int Sys_Min_Int(int a, int b) { return (a < b) ? a : b; }
SYS_MATH_INLINE int Sys_Max_Int(int a, int b) { return (a > b) ? a : b; }

SYS_MATH_INLINE long Sys_Min_Long(long a, long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE long Sys_Max_Long(long a, long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE long long Sys_Min_LLong(long long a, long long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE long long Sys_Max_LLong(long long a, long long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned int Sys_Min_UInt(unsigned int a, unsigned int b) { return (a < b) ? a : b; }
SYS_MATH_INLINE unsigned int Sys_Max_UInt(unsigned int a, unsigned int b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned long Sys_Min_ULong(unsigned long a, unsigned long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE unsigned long Sys_Max_ULong(unsigned long a, unsigned long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned long long Sys_Min_ULLong(unsigned long long a, unsigned long long b) { return (a < b) ? a : b; }
SYS_MATH_INLINE unsigned long long Sys_Max_ULLong(unsigned long long a, unsigned long long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE float Sys_Min_F32(float a, float b) { return (a < b) ? a : b; }
SYS_MATH_INLINE float Sys_Max_F32(float a, float b) { return (a > b) ? a : b; }

SYS_MATH_INLINE double Sys_Min_F64(double a, double b) { return (a < b) ? a : b; }
SYS_MATH_INLINE double Sys_Max_F64(double a, double b) { return (a > b) ? a : b; }

#define SYS_MIN(a, b) _Generic((a), \
  char:               Sys_Min_Int, \
  signed char:        Sys_Min_Int, \
  short:              Sys_Min_Int, \
  int:                Sys_Min_Int, \
  long:               Sys_Min_Long, \
  long long:          Sys_Min_LLong, \
  unsigned char:      Sys_Min_UInt, \
  unsigned short:     Sys_Min_UInt, \
  unsigned int:       Sys_Min_UInt, \
  unsigned long:      Sys_Min_ULong, \
  unsigned long long: Sys_Min_ULLong, \
  float:              Sys_Min_F32, \
  double:             Sys_Min_F64 \
)(a, b)

#define SYS_MAX(a, b) _Generic((a), \
  char:               Sys_Max_Int, \
  signed char:        Sys_Max_Int, \
  short:              Sys_Max_Int, \
  int:                Sys_Max_Int, \
  long:               Sys_Max_Long, \
  long long:          Sys_Max_LLong, \
  unsigned char:      Sys_Max_UInt, \
  unsigned short:     Sys_Max_UInt, \
  unsigned int:       Sys_Max_UInt, \
  unsigned long:      Sys_Max_ULong, \
  unsigned long long: Sys_Max_ULLong, \
  float:              Sys_Max_F32, \
  double:             Sys_Max_F64 \
)(a, b)

#endif //LAYERED_EMBEDDED_C_SYSTEM_MINMAX_H
