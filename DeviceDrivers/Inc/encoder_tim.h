//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_tim.h
 * @brief 基于定时器编码器模式的编码器实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_ENCODER_TIM_H
#define LAYERED_EMBEDDED_C_ENCODER_TIM_H

#include "encoder_base.h"
#include "platform_encoder.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 定时器编码器对象。
 * @note position 使用 int64 累计，降低长期溢出风险。
 * @note max_counts_per_period 为 0 时不限制单周期增量。
 */
typedef struct encoder_tim_s {
  encoder_base_t encoder_base;
  platform_encoder_instance_t tim;
  uint8_t bit_width;

  bool has_base_count;
  bool overflow;

  uint32_t max_counts_per_period; // 0 = 不限制

  int64_t position;
  uint32_t last_count;
} encoder_tim_t;

/**
 * @brief 初始化编码器对象、查询计数位宽并启动定时器编码器。
 * @param encoder_h 待初始化对象；非 NULL，生命周期和独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 encoder_base.name 保存指针而不复制。
 * @param tim 编码器定时器身份；必须是当前构建启用的 platform_encoder_instance_t。
 * @retval PLATFORM_STATUS_OK 位宽查询和编码器启动均成功，对象位宽为 16 或 32。
 * @retval PLATFORM_STATUS_ERROR 位宽查询或编码器启动失败，具体错误保留在 Platform 错误快照中。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 定时器身份无效，查询阶段即被 Platform 拒绝。
 * @pre 对应定时器已在 CubeMX/Core 中配置为 Encoder 模式，并已完成 MX_TIMx_Init()。
 * @pre 调用发生在控制任务启动前；encoder_h 和 name 为 NULL 时由调试断言拦截。
 * @post 成功时 encoder_base 已绑定操作表，position 和 last_count 清零，has_base_count 为 false，overflow 为 false，max_counts_per_period 为 0，硬件编码器已启动。
 * @post 位宽查询失败时仅 tim 已写入；位宽查询成功但启动失败时对象已完整初始化而外设未启动。失败路径均不隐藏状态，调用方应停止后续装配。
 * @note 不分配内存，不复制 name；首次 GetDelta 只建立硬件计数基线并返回 0，不产生累计位置。
 * @warning 非 ISR 安全；初始化后同一 encoder_h 的读写必须由单一控制上下文串行化。
 */
platform_status_t Dev_Encoder_TIM_Init(
  encoder_tim_t *encoder_h,
  const char *name,
  platform_encoder_instance_t tim
);
/**
 * @brief 设置单次增量读取允许的绝对计数上限。
 * @param encoder_h 已初始化的编码器对象；非 NULL。
 * @param max_counts 单次 GetDelta 允许的绝对增量，单位 counts；0 表示不限制，非零值同时限制正、负增量。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化；调用与 GetDelta/Resync/SetZero 在同一上下文串行执行。
 * @post max_counts_per_period 被直接保存；不读取硬件，不重置 last_count、has_base_count、position 或 overflow。
 * @note 上限在解码并完成回绕换算后应用；超过上限的增量会被夹紧到正/负上限，并锁存 overflow。
 * @note 新上限作用于下一次读取前尚未消费的计数，因此降低上限后可能立即触发夹紧。
 * @warning 无锁且非 ISR 安全；若在运动途中收紧上限，应先 Resync 建立新基线，避免把历史增量误判为超限。
 */
void Dev_Encoder_TIM_SetMaxCountsPerPeriod(encoder_tim_t *encoder_h, uint32_t max_counts);
/**
 * @brief 查询编码器是否锁存了超限或 Platform 操作错误。
 * @param encoder_h 已初始化的编码器对象；非 NULL，函数只读其 overflow 字段。
 * @retval true 自上次清零或成功 SetZero/Resync 后，曾发生增量夹紧或 Platform 计数操作失败。
 * @retval false 当前没有上述锁存条件。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化。
 * @post 不读取硬件、不清除标志、不修改 position 或计数基线。
 * @note overflow 同时表示“增量超过 max_counts_per_period”和“Platform 计数读写失败”，需结合 Platform_Encoder_GetLastError 区分原因。
 * @warning 非 ISR 安全且未使用原子访问；若其他上下文同时写 overflow，读取结果可能滞后。
 */
bool Dev_Encoder_TIM_HasOverflow(const encoder_tim_t *encoder_h);
/**
 * @brief 清除编码器锁存的超限或 Platform 操作错误标志。
 * @param encoder_h 已初始化的编码器对象；非 NULL。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化，且与读取操作在同一上下文串行执行。
 * @post overflow 被置为 false；position、last_count、has_base_count 和硬件计数器均不变。
 * @note 本函数不修复底层故障；若 Platform 读写仍失败，下一次 GetDelta/Resync/SetZero 会再次锁存 overflow。
 * @note 不分配内存、不访问硬件，也不清除 Platform 自身的最后错误快照。
 * @warning 无锁且非 ISR 安全；读取方可能观察到清除前后的混合状态，需由调用方保证互斥。
 */
void Dev_Encoder_TIM_ClearOverflow(encoder_tim_t *encoder_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_ENCODER_TIM_H
