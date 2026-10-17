//
// Created by Buns_ on 2026/10/18.
//

#ifndef LAYERED_EMBEDDED_C_SYSTEM_MATH_DEF_H
#define LAYERED_EMBEDDED_C_SYSTEM_MATH_DEF_H

#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#  define SYS_MATH_INLINE static inline __attribute__((always_inline))
#else
#  define SYS_MATH_INLINE static inline
#endif

typedef union { float  f; uint32_t u; } Sys_Math_F32_Bits;
typedef union { double f; uint64_t u; } Sys_Math_F64_Bits;

#endif //LAYERED_EMBEDDED_C_SYSTEM_MATH_DEF_H
