//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_base.h
 * @brief 编码器抽象：位置、增量与基线重同步。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_ENCODER_BASE_H
#define LAYERED_EMBEDDED_C_ENCODER_BASE_H

#include <stdint.h>

typedef struct encoder_base_s encoder_base_t;

/**
 * @brief 编码器具体实现的操作表。
 */
typedef struct encoder_ops_s {
  void    (*set_zero)     (encoder_base_t *encoder_h); // 设当前位置为零点
  int64_t (*get_position) (encoder_base_t *encoder_h); // 获取累计位置                   (count)
  int32_t (*get_delta)    (encoder_base_t *encoder_h); // 获取自上次采样的编码器的变化值 (count)
  void    (*resync)       (encoder_base_t *encoder_h);
} encoder_ops_t;

/**
 * @brief 编码器抽象基类。
 */
typedef struct encoder_base_s {
  const encoder_ops_t *ops;
  const char *name;
} encoder_base_t;

/**
 * @brief 绑定编码器名称与具体操作表。
 * @param encoder_h 调用方持有的编码器基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖编码器对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，具体操作在使用时按需校验。
 * @pre encoder_h 尚未并发发布，name 与 ops 在对象使用期间保持有效。
 * @post 只保存绑定，不清零计数、不读取硬件。
 * @note 仅限初始化阶段调用；不加锁且不分配内存，重复调用会覆盖已有绑定。
 */
void Dev_Encoder_Base_Init(encoder_base_t *encoder_h, const char *name, const encoder_ops_t *ops);
/**
 * @brief 委托具体编码器把当前位置设为零点。
 * @param encoder_h 已初始化的编码器句柄；不可为 NULL，所有权不转移。
 * @pre 已绑定非空 set_zero 操作；调用方已停止或串行化同对象的读取/采样。
 * @post 具体实现的累计位置基点变为 0；硬件计数器是否清零由实现决定。
 * @note 通常在任务上下文调用；本层不加锁，失败由具体实现处理且无错误码。
 * @warning 与 GetDelta、GetPosition 并发调用可能读到过渡状态。
 */
void Dev_Encoder_Base_SetZero(encoder_base_t *encoder_h);
/**
 * @brief 读取具体编码器维护的累计位置。
 * @param encoder_h 已初始化的编码器句柄；不可为 NULL，所有权不转移。
 * @return 相对具体实现零点的有符号累计计数，单位 counts，范围由 int64_t 限制。
 * @pre 已绑定非空 get_position 操作。
 * @post 本层不推进基线、不消费增量；具体实现不得在此调用中改变可观察状态。
 * @note 通常在控制任务读取；不加锁，须与 SetZero、GetDelta 串行化。
 * @warning 返回值没有错误哨兵，读取失败需由具体设备自身状态另行判断。
 */
int64_t Dev_Encoder_Base_GetPosition(encoder_base_t *encoder_h);
/**
 * @brief 读取并消费自上次采样以来的有符号增量。
 * @param encoder_h 已初始化的编码器句柄；不可为 NULL，所有权不转移。
 * @return 自上次读取以来检测到的计数差，单位 counts，范围受 int32_t 与具体限幅约束。
 * @pre 已绑定非空 get_delta 操作；同一控制周期尚未读取过该编码器。
 * @post 具体实现更新采样基线，下一次调用从本次读取时刻继续累计。
 * @warning 该接口消费增量；每周期只调用一次，遥测等旁路读取不得借用。
 * @note 不加锁，必须与位置读取和重同步操作串行化。
 */
int32_t Dev_Encoder_Base_GetDelta(encoder_base_t *encoder_h);
/**
 * @brief 委托具体编码器重新对齐增量基线。
 * @param encoder_h 已初始化的编码器句柄；不可为 NULL，所有权不转移。
 * @pre 已绑定非空 resync 操作；调用方已停止或串行化同对象的增量读取。
 * @post 下一次 GetDelta 以当前硬件计数为基线，累计位置通常保持不变。
 * @note 通常在使能或复位控制环时调用；不加锁，失败行为由具体实现定义。
 * @warning 不返回状态；底层计数读取失败只能通过具体设备状态观察。
 */
void Dev_Encoder_Base_Resync(encoder_base_t *encoder_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_ENCODER_BASE_H
