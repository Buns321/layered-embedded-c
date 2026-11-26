/**
 * @file sys_math.h
 * @brief 类型安全的绝对值、最值、限幅、符号与浮点有限性检查宏。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_MATH_H
#define LAYERED_CAR_SYS_MATH_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define SYS_MATH_INLINE static inline __attribute__((always_inline))
#else
#define SYS_MATH_INLINE static inline
#endif

/**
 * @brief 单精度浮点位模式联合体。
 */
typedef union sys_math_f32_bits_u {
  float f;
  uint32_t u;
} sys_math_f32_bits_t;

/**
 * @brief 双精度浮点位模式联合体。
 */
typedef union sys_math_f64_bits_u {
  double f;
  uint64_t u;
} sys_math_f64_bits_t;

/* ---- 绝对值（整数饱和版本防最小负值溢出） ---- */

SYS_MATH_INLINE int Sys_AbsInt(int x) { return (x < 0) ? -x : x; }

SYS_MATH_INLINE long Sys_AbsLong(long x) { return (x < 0) ? -x : x; }

SYS_MATH_INLINE long long Sys_AbsLLong(long long x) { return (x < 0) ? -x : x; }

SYS_MATH_INLINE unsigned int Sys_AbsUInt(unsigned int x) { return x; }

SYS_MATH_INLINE unsigned long Sys_AbsULong(unsigned long x) { return x; }

SYS_MATH_INLINE unsigned long long Sys_AbsULLong(unsigned long long x) { return x; }

SYS_MATH_INLINE float Sys_AbsF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  v.u &= 0x7FFFFFFFU;
  return v.f;
}

SYS_MATH_INLINE double Sys_AbsF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  v.u &= 0x7FFFFFFFFFFFFFFFULL;
  return v.f;
}

SYS_MATH_INLINE int Sys_AbsIntSat(int x) { return (x == INT_MIN) ? INT_MAX : ((x < 0) ? -x : x); }

SYS_MATH_INLINE long Sys_AbsLongSat(long x) {
  return (x == LONG_MIN) ? LONG_MAX : ((x < 0) ? -x : x);
}

SYS_MATH_INLINE long long Sys_AbsLLongSat(long long x) {
  return (x == LLONG_MIN) ? LLONG_MAX : ((x < 0) ? -x : x);
}

/** @brief 类型安全饱和绝对值（参数仅求值一次）。 */
#define SYS_ABS(x)                                                                                 \
  _Generic((x),                                                                                    \
      char: Sys_AbsIntSat,                                                                         \
      signed char: Sys_AbsIntSat,                                                                  \
      short: Sys_AbsIntSat,                                                                        \
      int: Sys_AbsIntSat,                                                                          \
      long: Sys_AbsLongSat,                                                                        \
      long long: Sys_AbsLLongSat,                                                                  \
      unsigned char: Sys_AbsUInt,                                                                  \
      unsigned short: Sys_AbsUInt,                                                                 \
      unsigned int: Sys_AbsUInt,                                                                   \
      unsigned long: Sys_AbsULong,                                                                 \
      unsigned long long: Sys_AbsULLong,                                                           \
      float: Sys_AbsF32,                                                                           \
      double: Sys_AbsF64)(x)

/* ---- 最小 / 最大值 ---- */

SYS_MATH_INLINE int Sys_MinInt(int a, int b) { return (a < b) ? a : b; }

SYS_MATH_INLINE int Sys_MaxInt(int a, int b) { return (a > b) ? a : b; }

SYS_MATH_INLINE long Sys_MinLong(long a, long b) { return (a < b) ? a : b; }

SYS_MATH_INLINE long Sys_MaxLong(long a, long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE long long Sys_MinLLong(long long a, long long b) { return (a < b) ? a : b; }

SYS_MATH_INLINE long long Sys_MaxLLong(long long a, long long b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned int Sys_MinUInt(unsigned int a, unsigned int b) { return (a < b) ? a : b; }

SYS_MATH_INLINE unsigned int Sys_MaxUInt(unsigned int a, unsigned int b) { return (a > b) ? a : b; }

SYS_MATH_INLINE unsigned long Sys_MinULong(unsigned long a, unsigned long b) {
  return (a < b) ? a : b;
}

SYS_MATH_INLINE unsigned long Sys_MaxULong(unsigned long a, unsigned long b) {
  return (a > b) ? a : b;
}

SYS_MATH_INLINE unsigned long long Sys_MinULLong(unsigned long long a, unsigned long long b) {
  return (a < b) ? a : b;
}

SYS_MATH_INLINE unsigned long long Sys_MaxULLong(unsigned long long a, unsigned long long b) {
  return (a > b) ? a : b;
}

SYS_MATH_INLINE float Sys_MinF32(float a, float b) { return (a < b) ? a : b; }

SYS_MATH_INLINE float Sys_MaxF32(float a, float b) { return (a > b) ? a : b; }

SYS_MATH_INLINE double Sys_MinF64(double a, double b) { return (a < b) ? a : b; }

SYS_MATH_INLINE double Sys_MaxF64(double a, double b) { return (a > b) ? a : b; }

/** @brief 类型安全最小值（各实参仅求值一次）。 */
#define SYS_MIN(a, b)                                                                              \
  _Generic((a),                                                                                    \
      char: Sys_MinInt,                                                                            \
      signed char: Sys_MinInt,                                                                     \
      short: Sys_MinInt,                                                                           \
      int: Sys_MinInt,                                                                             \
      long: Sys_MinLong,                                                                           \
      long long: Sys_MinLLong,                                                                     \
      unsigned char: Sys_MinUInt,                                                                  \
      unsigned short: Sys_MinUInt,                                                                 \
      unsigned int: Sys_MinUInt,                                                                   \
      unsigned long: Sys_MinULong,                                                                 \
      unsigned long long: Sys_MinULLong,                                                           \
      float: Sys_MinF32,                                                                           \
      double: Sys_MinF64)(a, b)

/** @brief 类型安全最大值（各实参仅求值一次）。 */
#define SYS_MAX(a, b)                                                                              \
  _Generic((a),                                                                                    \
      char: Sys_MaxInt,                                                                            \
      signed char: Sys_MaxInt,                                                                     \
      short: Sys_MaxInt,                                                                           \
      int: Sys_MaxInt,                                                                             \
      long: Sys_MaxLong,                                                                           \
      long long: Sys_MaxLLong,                                                                     \
      unsigned char: Sys_MaxUInt,                                                                  \
      unsigned short: Sys_MaxUInt,                                                                 \
      unsigned int: Sys_MaxUInt,                                                                   \
      unsigned long: Sys_MaxULong,                                                                 \
      unsigned long long: Sys_MaxULLong,                                                           \
      float: Sys_MaxF32,                                                                           \
      double: Sys_MaxF64)(a, b)

/* ---- 闭区间限幅 [lo, hi] ---- */

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

SYS_MATH_INLINE unsigned long long Sys_ClampULLong(unsigned long long x, unsigned long long lo,
                                                   unsigned long long hi) {
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

/** @brief 类型安全闭区间限幅，要求 lo <= hi（不检查 NaN）。 */
#define SYS_CLAMP(x, lo, hi)                                                                       \
  _Generic((x),                                                                                    \
      char: Sys_ClampInt,                                                                          \
      signed char: Sys_ClampInt,                                                                   \
      short: Sys_ClampInt,                                                                         \
      int: Sys_ClampInt,                                                                           \
      long: Sys_ClampLong,                                                                         \
      long long: Sys_ClampLLong,                                                                   \
      unsigned char: Sys_ClampUInt,                                                                \
      unsigned short: Sys_ClampUInt,                                                               \
      unsigned int: Sys_ClampUInt,                                                                 \
      unsigned long: Sys_ClampULong,                                                               \
      unsigned long long: Sys_ClampULLong,                                                         \
      float: Sys_ClampF32,                                                                         \
      double: Sys_ClampF64)(x, lo, hi)

/* ---- 符号函数 ---- */

SYS_MATH_INLINE int Sys_SignInt(int x) { return (x > 0) - (x < 0); }

SYS_MATH_INLINE long Sys_SignLong(long x) { return (long)((x > 0) - (x < 0)); }

SYS_MATH_INLINE long long Sys_SignLLong(long long x) { return (long long)((x > 0) - (x < 0)); }

SYS_MATH_INLINE unsigned int Sys_SignUInt(unsigned int x) { return (x != 0U) ? 1U : 0U; }

SYS_MATH_INLINE unsigned long Sys_SignULong(unsigned long x) { return (x != 0UL) ? 1UL : 0UL; }

SYS_MATH_INLINE unsigned long long Sys_SignULLong(unsigned long long x) {
  return (x != 0ULL) ? 1ULL : 0ULL;
}

SYS_MATH_INLINE float Sys_SignF32(float x) { return (float)((x > 0.0f) - (x < 0.0f)); }

SYS_MATH_INLINE double Sys_SignF64(double x) { return (double)((x > 0.0) - (x < 0.0)); }

/** @brief 类型安全符号函数，有符号返回 -1/0/1，无符号返回 0/1。 */
#define SYS_SIGN(x)                                                                                \
  _Generic((x),                                                                                    \
      char: Sys_SignInt,                                                                           \
      signed char: Sys_SignInt,                                                                    \
      short: Sys_SignInt,                                                                          \
      int: Sys_SignInt,                                                                            \
      long: Sys_SignLong,                                                                          \
      long long: Sys_SignLLong,                                                                    \
      unsigned char: Sys_SignUInt,                                                                 \
      unsigned short: Sys_SignUInt,                                                                \
      unsigned int: Sys_SignUInt,                                                                  \
      unsigned long: Sys_SignULong,                                                                \
      unsigned long long: Sys_SignULLong,                                                          \
      float: Sys_SignF32,                                                                          \
      double: Sys_SignF64)(x)

/* ---- IEEE 754 浮点有限性检查（位级检查，不触发浮点比较异常） ---- */

SYS_MATH_INLINE bool Sys_IsNanF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return ((v.u & 0x7F800000U) == 0x7F800000U) && ((v.u & 0x007FFFFFU) != 0U);
}

SYS_MATH_INLINE bool Sys_IsInfF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFU) == 0x7F800000U;
}

SYS_MATH_INLINE bool Sys_IsFiniteF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7F800000U) != 0x7F800000U;
}

SYS_MATH_INLINE bool Sys_IsNanF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return ((v.u & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL) &&
         ((v.u & 0x000FFFFFFFFFFFFFULL) != 0ULL);
}

SYS_MATH_INLINE bool Sys_IsInfF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFFFFFFFFFULL) == 0x7FF0000000000000ULL;
}

SYS_MATH_INLINE bool Sys_IsFiniteF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FF0000000000000ULL) != 0x7FF0000000000000ULL;
}

SYS_MATH_INLINE bool Sys_IsPositiveFiniteF32(float x) { return Sys_IsFiniteF32(x) && x > 0.0f; }

SYS_MATH_INLINE bool Sys_IsPositiveFiniteF64(double x) { return Sys_IsFiniteF64(x) && x > 0.0; }

SYS_MATH_INLINE bool Sys_IsNonNegativeFiniteF32(float x) { return Sys_IsFiniteF32(x) && x >= 0.0f; }

SYS_MATH_INLINE bool Sys_IsNonNegativeFiniteF64(double x) { return Sys_IsFiniteF64(x) && x >= 0.0; }

/** @brief 判断 float/double 是否为 NaN。 */
#define SYS_ISNAN(x) _Generic((x), float: Sys_IsNanF32, double: Sys_IsNanF64)(x)

/** @brief 判断 float/double 是否为正负无穷。 */
#define SYS_ISINF(x) _Generic((x), float: Sys_IsInfF32, double: Sys_IsInfF64)(x)

/** @brief 判断 float/double 是否为有限值。 */
#define SYS_ISFINITE(x) _Generic((x), float: Sys_IsFiniteF32, double: Sys_IsFiniteF64)(x)

/** @brief 判断 float/double 是否严格大于 0 且有限。 */
#define SYS_IS_POS_FINITE(x)                                                                       \
  _Generic((x), float: Sys_IsPositiveFiniteF32, double: Sys_IsPositiveFiniteF64)(x)

/** @brief 判断 float/double 是否非负且有限。 */
#define SYS_IS_NONNEG_FINITE(x)                                                                    \
  _Generic((x), float: Sys_IsNonNegativeFiniteF32, double: Sys_IsNonNegativeFiniteF64)(x)

/** @} */

#endif /* LAYERED_CAR_SYS_MATH_H */
