//
// 由 Buns_ 创建于 2026/10/18。
//

/**
 * @file system_clamp.h
 * @brief 类型安全的限幅函数与 SYS_CLAMP 宏。
 * @addtogroup system
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H
#define LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H

#include "system_math_def.h"
#include <assert.h>

/**
 * @brief 把有符号整数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE int Sys_ClampInt(int x, int lo, int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把长整数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE long Sys_ClampLong(long x, long lo, long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把长长整数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE long long Sys_ClampLLong(long long x, long long lo, long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把无符号整数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE unsigned int Sys_ClampUInt(unsigned int x, unsigned int lo, unsigned int hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把无符号长整数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE unsigned long Sys_ClampULong(unsigned long x, unsigned long lo, unsigned long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把无符号长长整数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE unsigned long long Sys_ClampULLong(unsigned long long x, unsigned long long lo, unsigned long long hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把单精度浮点数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE float Sys_ClampF32(float x, float lo, float hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 把双精度浮点数限制在闭区间 [lo, hi]。
 * @param x 待限制的值。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制后的值。
 * @warning 调用方必须保证 lo <= hi；Debug 构建由 assert 检查。
 */
SYS_MATH_INLINE double Sys_ClampF64(double x, double lo, double hi) {
  assert(lo <= hi);
  return (x < lo) ? lo : ((x > hi) ? hi : x);
}

/**
 * @brief 按输入类型把数值限制在闭区间 [lo, hi]。
 * @param x 待限制的值，其类型用于选择具体实现。
 * @param lo 下限。
 * @param hi 上限。
 * @return 限制到闭区间后的值。
 * @pre lo 必须小于或等于 hi。
 * @warning 不处理 NaN；浮点输入应先完成有限性检查。Debug 构建由 assert
 *          检查区间前置条件，Release 构建仍要求调用方保证条件成立。
 * @note `_Generic` 控制表达式不求值，各实参只求值一次。
 */
#define SYS_CLAMP(x, lo, hi) _Generic((x), \
  char:               Sys_ClampInt, \
  signed char:        Sys_ClampInt, \
  short:              Sys_ClampInt, \
  int:                Sys_ClampInt, \
  long:               Sys_ClampLong, \
  long long:          Sys_ClampLLong, \
  unsigned char:      Sys_ClampUInt, \
  unsigned short:     Sys_ClampUInt, \
  unsigned int:       Sys_ClampUInt, \
  unsigned long:      Sys_ClampULong, \
  unsigned long long: Sys_ClampULLong, \
  float:              Sys_ClampF32, \
  double:             Sys_ClampF64 \
)(x, lo, hi)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_CLAMP_H
