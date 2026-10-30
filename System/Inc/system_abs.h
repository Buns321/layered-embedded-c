//
// 由 Buns_ 创建于 2026/8/16。
//

/**
 * @file system_abs.h
 * @brief 类型安全的绝对值，含整数饱和版本。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_ABS_H
#define LAYERED_EMBEDDED_C_SYSTEM_ABS_H

#include "system_math_def.h"
#include <limits.h>

/* 有符号整数：条件取反，Cortex-M 上编译为 IT + RSB，无跳转 */
/**
 * @brief 返回有符号整数的绝对值。
 * @param x 待取绝对值的 int 值。
 * @return |x|；除最小值外均在 int 可表示范围内。
 * @warning 输入类型最小负值时普通版本会溢出，应改用饱和版本。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE int Sys_AbsInt(int x) {
  return (x < 0) ? -x : x;
}

/**
 * @brief 返回长整数的绝对值。
 * @param x 待取绝对值的 long 值。
 * @return |x|；除 LONG_MIN 外均在 long 可表示范围内。
 * @warning 输入类型最小负值时普通版本会溢出，应改用饱和版本。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE long Sys_AbsLong(long x) {
  return (x < 0) ? -x : x;
}

/**
 * @brief 返回长长整数的绝对值。
 * @param x 待取绝对值的 long long 值。
 * @return |x|；除 LLONG_MIN 外均在 long long 可表示范围内。
 * @warning 输入类型最小负值时普通版本会溢出，应改用饱和版本。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE long long Sys_AbsLLong(long long x) {
  return (x < 0) ? -x : x;
}

/* 无符号整数：原样返回 */
/**
 * @brief 返回无符号整数的绝对值。
 * @param x 待返回的 unsigned int 值；无符号语义下已非负。
 * @return x 本身；无符号值已非负，无需取反。
 * @note 纯函数，不检查溢出也不修改输入；不分配动态内存。
 */
SYS_MATH_INLINE unsigned int Sys_AbsUInt(unsigned int x) { return x; }
/**
 * @brief 返回无符号长整数的绝对值。
 * @param x 待返回的 unsigned long 值；无符号语义下已非负。
 * @return x 本身；无符号值已非负，无需取反。
 * @note 纯函数，不检查溢出也不修改输入；不分配动态内存。
 */
SYS_MATH_INLINE unsigned long Sys_AbsULong(unsigned long x) { return x; }
/**
 * @brief 返回无符号长长整数的绝对值。
 * @param x 待返回的 unsigned long long 值；无符号语义下已非负。
 * @return x 本身；无符号值已非负，无需取反。
 * @note 纯函数，不检查溢出也不修改输入；不分配动态内存。
 */
SYS_MATH_INLINE unsigned long long Sys_AbsULLong(unsigned long long x) { return x; }

/* 浮点：清符号位，不依赖 FPU */
/**
 * @brief 返回单精度浮点数的绝对值。
 * @param x 待处理的 float 值。
 * @return 清除符号位后的 float 幅值。
 * @note 不检查 NaN/Inf，非有限输入的符号位同样会被清除；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE float Sys_AbsF32(float x) {
  sys_math_f32_bits_t v;
  v.f = x;
  v.u &= 0x7FFFFFFFu;
  return v.f;
}

/**
 * @brief 返回双精度浮点数的绝对值。
 * @param x 待处理的 double 值。
 * @return 清除符号位后的 double 幅值。
 * @note 不检查 NaN/Inf，非有限输入的符号位同样会被清除；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE double Sys_AbsF64(double x) {
  sys_math_f64_bits_t v;
  v.f = x;
  v.u &= 0x7FFFFFFFFFFFFFFFull;
  return v.f;
}

/* 饱和版本：最小值取绝对值后返回最大值，避免溢出 */
/**
 * @brief 返回有符号整数的饱和绝对值。
 * @param x 待取绝对值的 int 值。
 * @return 普通输入返回 |x|；类型最小负值返回对应最大值。
 * @note INT_MIN 返回 INT_MAX 而非未定义的 -INT_MIN；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE int Sys_AbsIntSat(int x) {
  return (x == INT_MIN) ? INT_MAX : ((x < 0) ? -x : x);
}

/**
 * @brief 返回长整数的饱和绝对值。
 * @param x 待取绝对值的 long 值。
 * @return 普通输入返回 |x|；类型最小负值返回对应最大值。
 * @note LONG_MIN 返回 LONG_MAX 而非未定义的 -LONG_MIN；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE long Sys_AbsLongSat(long x) {
  return (x == LONG_MIN) ? LONG_MAX : ((x < 0) ? -x : x);
}

/**
 * @brief 返回长长整数的饱和绝对值。
 * @param x 待取绝对值的 long long 值。
 * @return 普通输入返回 |x|；类型最小负值返回对应最大值。
 * @note LLONG_MIN 返回 LLONG_MAX 而非未定义的 -LLONG_MIN；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE long long Sys_AbsLLongSat(long long x) {
  return (x == LLONG_MIN) ? LLONG_MAX : ((x < 0) ? -x : x);
}

/* 通用入口：C11 _Generic，表达式只求值一次 */
/**
 * @brief 按输入类型返回饱和绝对值。
 * @param x 有符号整数、无符号整数或浮点值。
 * @return 有符号类型的最小负值饱和为对应最大值；其余返回 |x|。
 * @note `_Generic` 控制表达式不求值，x 作为实参只求值一次。
 */
#define SYS_ABS(x) _Generic((x), \
  char:               Sys_AbsIntSat, \
  signed char:        Sys_AbsIntSat, \
  short:              Sys_AbsIntSat, \
  int:                Sys_AbsIntSat, \
  long:               Sys_AbsLongSat, \
  long long:          Sys_AbsLLongSat, \
  unsigned char:      Sys_AbsUInt, \
  unsigned short:     Sys_AbsUInt, \
  unsigned int:       Sys_AbsUInt, \
  unsigned long:      Sys_AbsULong, \
  unsigned long long: Sys_AbsULLong, \
  float:              Sys_AbsF32, \
  double:             Sys_AbsF64 \
)(x)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_ABS_H
