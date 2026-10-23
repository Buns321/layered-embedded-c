//
// 由 Buns_ 创建于 2026/10/18。
//

/**
 * @file system_math_def.h
 * @brief 数学工具的内联宏与浮点位表示。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_MATH_DEF_H
#define LAYERED_EMBEDDED_C_SYSTEM_MATH_DEF_H

#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#  define SYS_MATH_INLINE static inline __attribute__((always_inline))
#else
#  define SYS_MATH_INLINE static inline
#endif

typedef union sys_math_f32_bits_u { float  f; uint32_t u; } sys_math_f32_bits_t;
typedef union sys_math_f64_bits_u { double f; uint64_t u; } sys_math_f64_bits_t;

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_MATH_DEF_H
