/**
 * @file sys_math.h
 * @brief 类型安全的绝对值、最值、限幅、符号与浮点有限性检查宏。
 * @details 属于 Layer 2（Framework/Core），基于 ISO C11 `_Generic` 与内联函数实现
 *          单次求值的类型安全数学原语，零 HAL/RTOS 依赖，可供 Control 层直接包含。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_MATH_H
#define LAYERED_CAR_SYS_MATH_H

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
/** @brief 强制内联限定符（GCC/Clang 下启用 always_inline）。 */
#define SYS_MATH_INLINE static inline __attribute__((always_inline))
#else
/** @brief 强制内联限定符（标准 C11 static inline 回退）。 */
#define SYS_MATH_INLINE static inline
#endif

/**
 * @brief 单精度浮点位模式联合体（用于无异常位级有限性检查与符号位清除）。
 */
typedef union sys_math_f32_bits_u {
  float f;    /**< 32 位单精度浮点视图。 */
  uint32_t u; /**< 32 位 IEEE 754 无符号整数位模式视图。 */
} sys_math_f32_bits_t;

/**
 * @brief 双精度浮点位模式联合体（用于无异常位级有限性检查与符号位清除）。
 */
typedef union sys_math_f64_bits_u {
  double f;   /**< 64 位双精度浮点视图。 */
  uint64_t u; /**< 64 位 IEEE 754 无符号整数位模式视图。 */
} sys_math_f64_bits_t;

/* ---- 绝对值（整数饱和版本防最小负值溢出） ---- */

/** @brief 计算 int 绝对值。 @param[in] x 输入值。 @return 绝对值。 */
SYS_MATH_INLINE int Sys_AbsInt(int x) { return (x < 0) ? -x : x; }

/** @brief 计算 long 绝对值。 @param[in] x 输入值。 @return 绝对值。 */
SYS_MATH_INLINE long Sys_AbsLong(long x) { return (x < 0) ? -x : x; }

/** @brief 计算 long long 绝对值。 @param[in] x 输入值。 @return 绝对值。 */
SYS_MATH_INLINE long long Sys_AbsLLong(long long x) { return (x < 0) ? -x : x; }

/** @brief 计算 unsigned int 绝对值（恒等返回）。 @param[in] x 输入值。 @return 原值。 */
SYS_MATH_INLINE unsigned int Sys_AbsUInt(unsigned int x) { return x; }

/** @brief 计算 unsigned long 绝对值（恒等返回）。 @param[in] x 输入值。 @return 原值。 */
SYS_MATH_INLINE unsigned long Sys_AbsULong(unsigned long x) { return x; }

/** @brief 计算 unsigned long long 绝对值（恒等返回）。 @param[in] x 输入值。 @return 原值。 */
SYS_MATH_INLINE unsigned long long Sys_AbsULLong(unsigned long long x) { return x; }

/** @brief 通过清除 IEEE 754 符号位计算 float 绝对值。 @param[in] x 输入浮点值。 @return 绝对值。 */
SYS_MATH_INLINE float Sys_AbsF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  v.u &= 0x7FFFFFFFU;
  return v.f;
}

/** @brief 通过清除 IEEE 754 符号位计算 double 绝对值。 @param[in] x 输入浮点值。 @return 绝对值。
 */
SYS_MATH_INLINE double Sys_AbsF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  v.u &= 0x7FFFFFFFFFFFFFFFULL;
  return v.f;
}

/** @brief 计算 int 饱和绝对值（INT_MIN 饱和至 INT_MAX）。 @param[in] x 输入值。 @return
 * 饱和绝对值。 */
SYS_MATH_INLINE int Sys_AbsIntSat(int x) { return (x == INT_MIN) ? INT_MAX : ((x < 0) ? -x : x); }

/** @brief 计算 long 饱和绝对值（LONG_MIN 饱和至 LONG_MAX）。 @param[in] x 输入值。 @return
 * 饱和绝对值。 */
SYS_MATH_INLINE long Sys_AbsLongSat(long x) {
  return (x == LONG_MIN) ? LONG_MAX : ((x < 0) ? -x : x);
}

/** @brief 计算 long long 饱和绝对值（LLONG_MIN 饱和至 LLONG_MAX）。 @param[in] x 输入值。 @return
 * 饱和绝对值。 */
SYS_MATH_INLINE long long Sys_AbsLLongSat(long long x) {
  return (x == LLONG_MIN) ? LLONG_MAX : ((x < 0) ? -x : x);
}

/**
 * @brief 类型安全饱和绝对值（参数仅求值一次）。
 * @param x 标量数值表达式。
 */
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

/** @brief 返回两个 int 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较小值。 */
SYS_MATH_INLINE int Sys_MinInt(int a, int b) { return (a < b) ? a : b; }

/** @brief 返回两个 int 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较大值。 */
SYS_MATH_INLINE int Sys_MaxInt(int a, int b) { return (a > b) ? a : b; }

/** @brief 返回两个 long 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较小值。 */
SYS_MATH_INLINE long Sys_MinLong(long a, long b) { return (a < b) ? a : b; }

/** @brief 返回两个 long 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较大值。 */
SYS_MATH_INLINE long Sys_MaxLong(long a, long b) { return (a > b) ? a : b; }

/** @brief 返回两个 long long 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较小值。 */
SYS_MATH_INLINE long long Sys_MinLLong(long long a, long long b) { return (a < b) ? a : b; }

/** @brief 返回两个 long long 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较大值。 */
SYS_MATH_INLINE long long Sys_MaxLLong(long long a, long long b) { return (a > b) ? a : b; }

/** @brief 返回两个 unsigned int 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。
 * @return 较小值。 */
SYS_MATH_INLINE unsigned int Sys_MinUInt(unsigned int a, unsigned int b) { return (a < b) ? a : b; }

/** @brief 返回两个 unsigned int 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。
 * @return 较大值。 */
SYS_MATH_INLINE unsigned int Sys_MaxUInt(unsigned int a, unsigned int b) { return (a > b) ? a : b; }

/** @brief 返回两个 unsigned long 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。
 * @return 较小值。 */
SYS_MATH_INLINE unsigned long Sys_MinULong(unsigned long a, unsigned long b) {
  return (a < b) ? a : b;
}

/** @brief 返回两个 unsigned long 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。
 * @return 较大值。 */
SYS_MATH_INLINE unsigned long Sys_MaxULong(unsigned long a, unsigned long b) {
  return (a > b) ? a : b;
}

/** @brief 返回两个 unsigned long long 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。
 * @return 较小值。 */
SYS_MATH_INLINE unsigned long long Sys_MinULLong(unsigned long long a, unsigned long long b) {
  return (a < b) ? a : b;
}

/** @brief 返回两个 unsigned long long 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。
 * @return 较大值。 */
SYS_MATH_INLINE unsigned long long Sys_MaxULLong(unsigned long long a, unsigned long long b) {
  return (a > b) ? a : b;
}

/** @brief 返回两个 float 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较小值。 */
SYS_MATH_INLINE float Sys_MinF32(float a, float b) { return (a < b) ? a : b; }

/** @brief 返回两个 float 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较大值。 */
SYS_MATH_INLINE float Sys_MaxF32(float a, float b) { return (a > b) ? a : b; }

/** @brief 返回两个 double 中的较小值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较小值。 */
SYS_MATH_INLINE double Sys_MinF64(double a, double b) { return (a < b) ? a : b; }

/** @brief 返回两个 double 中的较大值。 @param[in] a 左操作数。 @param[in] b 右操作数。 @return
 * 较大值。 */
SYS_MATH_INLINE double Sys_MaxF64(double a, double b) { return (a > b) ? a : b; }

/**
 * @brief 类型安全最小值（各实参仅求值一次）。
 * @param a 左操作数。
 * @param b 右操作数。
 */
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

/**
 * @brief 类型安全最大值（各实参仅求值一次）。
 * @param a 左操作数。
 * @param b 右操作数。
 */
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

/** @brief 将 int 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in] hi 上限。
 * @return 限幅结果。 */
SYS_MATH_INLINE int Sys_ClampInt(int x, int lo, int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 long 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in] hi 上限。
 * @return 限幅结果。 */
SYS_MATH_INLINE long Sys_ClampLong(long x, long lo, long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 long long 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in] hi
 * 上限。 @return 限幅结果。 */
SYS_MATH_INLINE long long Sys_ClampLLong(long long x, long long lo, long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 unsigned int 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in]
 * hi 上限。 @return 限幅结果。 */
SYS_MATH_INLINE unsigned int Sys_ClampUInt(unsigned int x, unsigned int lo, unsigned int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 unsigned long 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in]
 * hi 上限。 @return 限幅结果。 */
SYS_MATH_INLINE unsigned long Sys_ClampULong(unsigned long x, unsigned long lo, unsigned long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 unsigned long long 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。
 * @param[in] hi 上限。 @return 限幅结果。 */
SYS_MATH_INLINE unsigned long long Sys_ClampULLong(unsigned long long x, unsigned long long lo,
                                                   unsigned long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 float 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in] hi
 * 上限。 @return 限幅结果。 */
SYS_MATH_INLINE float Sys_ClampF32(float x, float lo, float hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/** @brief 将 double 限幅至 [lo, hi]。 @param[in] x 输入值。 @param[in] lo 下限。 @param[in] hi
 * 上限。 @return 限幅结果。 */
SYS_MATH_INLINE double Sys_ClampF64(double x, double lo, double hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 类型安全闭区间限幅，要求 lo <= hi（不检查 NaN）。
 * @param x  输入值。
 * @param lo 闭区间下界。
 * @param hi 闭区间上界。
 */
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

/** @brief 返回 int 符号（-1 / 0 / 1）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE int Sys_SignInt(int x) { return (x > 0) - (x < 0); }

/** @brief 返回 long 符号（-1 / 0 / 1）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE long Sys_SignLong(long x) { return (long)((x > 0) - (x < 0)); }

/** @brief 返回 long long 符号（-1 / 0 / 1）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE long long Sys_SignLLong(long long x) { return (long long)((x > 0) - (x < 0)); }

/** @brief 返回 unsigned int 符号（0 或 1）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE unsigned int Sys_SignUInt(unsigned int x) { return (x != 0U) ? 1U : 0U; }

/** @brief 返回 unsigned long 符号（0 或 1）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE unsigned long Sys_SignULong(unsigned long x) { return (x != 0UL) ? 1UL : 0UL; }

/** @brief 返回 unsigned long long 符号（0 或 1）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE unsigned long long Sys_SignULLong(unsigned long long x) {
  return (x != 0ULL) ? 1ULL : 0ULL;
}

/** @brief 返回 float 符号（-1.0f / 0.0f / 1.0f）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE float Sys_SignF32(float x) { return (float)((x > 0.0f) - (x < 0.0f)); }

/** @brief 返回 double 符号（-1.0 / 0.0 / 1.0）。 @param[in] x 输入值。 @return 符号值。 */
SYS_MATH_INLINE double Sys_SignF64(double x) { return (double)((x > 0.0) - (x < 0.0)); }

/**
 * @brief 类型安全符号函数，有符号返回 -1/0/1，无符号返回 0/1。
 * @param x 标量数值表达式。
 */
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

/** @brief 位级判断 float 是否为 NaN。 @param[in] x 输入值。 @return 是否为 NaN。 */
SYS_MATH_INLINE bool Sys_IsNanF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return ((v.u & 0x7F800000U) == 0x7F800000U) && ((v.u & 0x007FFFFFU) != 0U);
}

/** @brief 位级判断 float 是否为正负无穷。 @param[in] x 输入值。 @return 是否为 Inf。 */
SYS_MATH_INLINE bool Sys_IsInfF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFU) == 0x7F800000U;
}

/** @brief 位级判断 float 是否为有限值（非 NaN 且非 Inf）。 @param[in] x 输入值。 @return 是否有限。
 */
SYS_MATH_INLINE bool Sys_IsFiniteF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  return (v.u & 0x7F800000U) != 0x7F800000U;
}

/** @brief 位级判断 double 是否为 NaN。 @param[in] x 输入值。 @return 是否为 NaN。 */
SYS_MATH_INLINE bool Sys_IsNanF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return ((v.u & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL) &&
         ((v.u & 0x000FFFFFFFFFFFFFULL) != 0ULL);
}

/** @brief 位级判断 double 是否为正负无穷。 @param[in] x 输入值。 @return 是否为 Inf。 */
SYS_MATH_INLINE bool Sys_IsInfF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FFFFFFFFFFFFFFFULL) == 0x7FF0000000000000ULL;
}

/** @brief 位级判断 double 是否为有限值（非 NaN 且非 Inf）。 @param[in] x 输入值。 @return
 * 是否有限。 */
SYS_MATH_INLINE bool Sys_IsFiniteF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  return (v.u & 0x7FF0000000000000ULL) != 0x7FF0000000000000ULL;
}

/** @brief 判断 float 是否严格大于 0 且有限。 @param[in] x 输入值。 @return 是否为正有限值。 */
SYS_MATH_INLINE bool Sys_IsPositiveFiniteF32(float x) { return Sys_IsFiniteF32(x) && x > 0.0f; }

/** @brief 判断 double 是否严格大于 0 且有限。 @param[in] x 输入值。 @return 是否为正有限值。 */
SYS_MATH_INLINE bool Sys_IsPositiveFiniteF64(double x) { return Sys_IsFiniteF64(x) && x > 0.0; }

/** @brief 判断 float 是否非负且有限。 @param[in] x 输入值。 @return 是否为非负有限值。 */
SYS_MATH_INLINE bool Sys_IsNonNegativeFiniteF32(float x) { return Sys_IsFiniteF32(x) && x >= 0.0f; }

/** @brief 判断 double 是否非负且有限。 @param[in] x 输入值。 @return 是否为非负有限值。 */
SYS_MATH_INLINE bool Sys_IsNonNegativeFiniteF64(double x) { return Sys_IsFiniteF64(x) && x >= 0.0; }

/** @brief 判断 float/double 是否为 NaN。 @param x 浮点表达式。 */
#define SYS_ISNAN(x) _Generic((x), float: Sys_IsNanF32, double: Sys_IsNanF64)(x)

/** @brief 判断 float/double 是否为正负无穷。 @param x 浮点表达式。 */
#define SYS_ISINF(x) _Generic((x), float: Sys_IsInfF32, double: Sys_IsInfF64)(x)

/** @brief 判断 float/double 是否为有限值。 @param x 浮点表达式。 */
#define SYS_ISFINITE(x) _Generic((x), float: Sys_IsFiniteF32, double: Sys_IsFiniteF64)(x)

/** @brief 判断 float/double 是否严格大于 0 且有限。 @param x 浮点表达式。 */
#define SYS_IS_POS_FINITE(x)                                                                       \
  _Generic((x), float: Sys_IsPositiveFiniteF32, double: Sys_IsPositiveFiniteF64)(x)

/** @brief 判断 float/double 是否非负且有限。 @param x 浮点表达式。 */
#define SYS_IS_NONNEG_FINITE(x)                                                                    \
  _Generic((x), float: Sys_IsNonNegativeFiniteF32, double: Sys_IsNonNegativeFiniteF64)(x)

/** @} */

#endif /* LAYERED_CAR_SYS_MATH_H */
