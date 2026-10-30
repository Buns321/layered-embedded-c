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

/**
 * @brief 返回有符号整数的符号。
 * @param x 待判断的 int 值。
 * @return x 小于、等于、大于 0 时分别为 -1、0、1。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE int Sys_SignInt(int x) { return (x > 0) - (x < 0); }
/**
 * @brief 返回长整数的符号。
 * @param x 待判断的 long 值。
 * @return x 小于、等于、大于 0 时分别为 -1、0、1。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE long Sys_SignLong(long x) { return (long)((x > 0) - (x < 0)); }
/**
 * @brief 返回长长整数的符号。
 * @param x 待判断的 long long 值。
 * @return x 小于、等于、大于 0 时分别为 -1、0、1。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE long long Sys_SignLLong(long long x) { return (long long)((x > 0) - (x < 0)); }

/**
 * @brief 返回无符号整数是否非零。
 * @param x 待判断的无符号整数值。
 * @return x 非零返回 1，否则返回 0。
 * @note 无符号类型没有负号，因此结果仅为 0 或 1；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE unsigned int Sys_SignUInt(unsigned int x) { return (x != 0u) ? 1u : 0u; }
/**
 * @brief 返回无符号长整数是否非零。
 * @param x 待判断的无符号长整数值。
 * @return x 非零返回 1，否则返回 0。
 * @note 无符号类型没有负号，因此结果仅为 0 或 1；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE unsigned long Sys_SignULong(unsigned long x) { return (x != 0ul) ? 1ul : 0ul; }
/**
 * @brief 返回无符号长长整数是否非零。
 * @param x 待判断的无符号长长整数值。
 * @return x 非零返回 1，否则返回 0。
 * @note 无符号类型没有负号，因此结果仅为 0 或 1；纯函数，不分配动态内存。
 */
SYS_MATH_INLINE unsigned long long Sys_SignULLong(unsigned long long x) { return (x != 0ull) ? 1ull : 0ull; }

/**
 * @brief 返回单精度浮点数的符号。
 * @param x 待判断的 float 值。
 * @return x 小于、等于、大于 0 时分别为 -1、0、1。
 * @warning NaN 与 0 的比较均为假，因此返回 0；调用方应先完成有限性检查。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE float Sys_SignF32(float x) { return (float)((x > 0.0f) - (x < 0.0f)); }
/**
 * @brief 返回双精度浮点数的符号。
 * @param x 待判断的 double 值。
 * @return x 小于、等于、大于 0 时分别为 -1、0、1。
 * @warning NaN 与 0 的比较均为假，因此返回 0；调用方应先完成有限性检查。
 * @note 纯函数，不修改输入或共享状态；不分配动态内存。
 */
SYS_MATH_INLINE double Sys_SignF64(double x) { return (double)((x > 0.0) - (x < 0.0)); }

/**
 * @brief 按输入类型返回数值的符号。
 * @param x 有符号整数、无符号整数或浮点表达式，其类型用于选择具体实现。
 * @return 有符号输入小于、等于、大于 0 时分别为 -1、0、1；无符号输入返回 0 或 1。
 * @warning 浮点 NaN 与 0 的比较均为假，因此返回 0；调用方应先完成有限性检查。
 * @note `_Generic` 控制表达式不求值，x 作为实参只求值一次。
 */
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
