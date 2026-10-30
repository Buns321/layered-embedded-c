//
// 由 Buns_ 创建于 2026/8/30。
//

/**
 * @file encoder_tim.c
 * @brief 基于 TIM 的编码器驱动实现。
 * @ingroup drivers_concrete
 */

#include "encoder_tim.h"
#include "platform_encoder.h"
#include "assert.h"

/**
 * @brief 读取硬件计数，解码回绕增量并更新累计位置。
 * @param encoder_h 编码器的 encoder_base 视图；非 NULL，且必须来自已初始化的 encoder_tim_t 首位成员。
 * @return 自上次成功读取以来的带符号增量，单位 counts；正数表示计数增加，负数表示减少。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 完成初始化，bit_width 为 16 或 32，且 encoder_h 未被并发访问。
 * @pre 每次控制周期只允许一个消费者调用；运动方向不确定时，两次读取之间的真实计数变化必须小于计数器量程的一半。
 * @post 成功时 last_count 更新为本次原始计数，position 增加解码并夹紧后的 delta；首次调用只建立基线并返回 0。
 * @post Platform_Encoder_GetCounter 失败时返回 0、锁存 overflow 为 true，并保留 last_count 与 position；下一次成功读取会跨越整个失败区间计算增量。
 * @note 16 位定时器对差值取低 16 位并把 >= 0x8000 解释为负数；32 位定时器把 >= 0x80000000 解释为负数。
 * @note max_counts_per_period 非零时，解码后的正负增量分别被夹紧到该上限，溢出不会扩展为 int64 返回。
 * @warning 真实变化达到或超过半个量程时回绕方向会别名，本函数无法识别；不使用锁或临界区，不得由 ISR 与任务并发调用。
 */
static int32_t dev_encoder_tim_readdelta(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  uint32_t now = 0u;
  if (Platform_Encoder_GetCounter(encoder_tim_h->tim, &now) != PLATFORM_STATUS_OK) {
    encoder_tim_h->overflow = true;
    return 0;
  }
  if (encoder_tim_h->has_base_count == false) {
    encoder_tim_h->last_count = now;
    encoder_tim_h->has_base_count = true;
    return 0;
  }

  uint32_t diff = now - encoder_tim_h->last_count;
  int32_t delta;

  if (encoder_tim_h->bit_width == 16U) {
    diff &= 0xFFFFU;
    if (diff >= 0x8000U) {
      delta = (int32_t)((int64_t)diff - 0x10000LL);
    } else {
      delta = (int32_t)diff;
    }
  } else {
    if (diff >= 0x80000000U) {
      delta = (int32_t)((int64_t)diff - 0x100000000LL);
    } else {
      delta = (int32_t)diff;
    }
  }

  if (encoder_tim_h->max_counts_per_period != 0u) {
    int64_t delta64 = (int64_t)delta;
    int64_t limit = (int64_t)encoder_tim_h->max_counts_per_period;

    if (delta64 > limit) {
      delta64 = limit;
      encoder_tim_h->overflow = true;
    } else if (delta64 < -limit) {
      delta64 = -limit;
      encoder_tim_h->overflow = true;
    }

    delta = (int32_t)delta64;
  }

  encoder_tim_h->last_count = now;
  encoder_tim_h->position += delta;
  return delta;
}

/**
 * @brief 返回已累计的编码器位置。
 * @param encoder_h 编码器的 encoder_base 视图；非 NULL，且必须来自已初始化的 encoder_tim_t 首位成员。
 * @return 自初始化或最近一次成功 SetZero 以来累计的计数，单位 counts；范围为 int64_t。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化。
 * @post 不访问硬件、不消费增量，也不修改 last_count、overflow 或 position。
 * @note 返回值基于最近一次成功 GetDelta 更新；硬件中尚未读取的计数不在其中，读取失败期间的位置保持不变。
 * @warning 无锁；与 GetDelta、SetZero、Resync 跨上下文并发时可能读到更新中的字段，非 ISR 安全。
 */
static int64_t dev_encoder_tim_getposition(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  return encoder_tim_h->position;
}

/**
 * @brief 将硬件计数器、累计位置和增量基线同时归零。
 * @param encoder_h 编码器的 encoder_base 视图；非 NULL，且必须来自已初始化的 encoder_tim_t 首位成员。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化；建议在停车或已同步状态下调用，且不与 GetDelta 并发。
 * @post 成功时硬件计数器被写为 0，last_count 为 0，has_base_count 为 true，position 为 0，overflow 被清除。
 * @post Platform_Encoder_SetCounter 失败时仅锁存 overflow 为 true；旧 position、last_count 和基数状态均保持不变。
 * @note 本函数不返回状态；失败可通过 HasOverflow 与 Platform_Encoder_GetLastError 检查。
 * @warning 会丢弃尚未读取的计数并重建零点；无锁且非 ISR 安全，不得与同一编码器的其他读取并发。
 */
static void dev_encoder_tim_setzero(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  if (Platform_Encoder_SetCounter(encoder_tim_h->tim, 0u) != PLATFORM_STATUS_OK) {
    encoder_tim_h->overflow = true;
    return;
  }
  encoder_tim_h->last_count = 0;
  encoder_tim_h->has_base_count = true;
  encoder_tim_h->overflow = false;
  encoder_tim_h->position = 0;
}

/**
 * @brief 读取并消费自上次调用以来的编码器增量。
 * @param encoder_h 编码器的 encoder_base 视图；非 NULL，且必须来自已初始化的 encoder_tim_t 首位成员。
 * @return 自上次成功读取以来的带符号增量，单位 counts；首次调用或 Platform 读取失败时返回 0。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化，且每个控制周期只有一个消费者调用该接口。
 * @post 成功时更新 last_count 和 position，并把增量返回给调用方；首次调用仅建立基线，不影响 position。
 * @post Platform 失败时锁存 overflow 且不推进基线，后续成功读取会包含失败期间累积的计数。
 * @note 返回 0 不能单独区分“真实无位移”“首次建基线”和“读取失败”，应结合 HasOverflow 判断。
 * @warning 该接口消费增量；同一周期重复调用通常得到 0，且会破坏闭环速度计算。无锁，非 ISR 安全。
 */
static int32_t dev_encoder_tim_getdelta(encoder_base_t *encoder_h) {
  assert(encoder_h);
  return dev_encoder_tim_readdelta(encoder_h);
}

/**
 * @brief 重新对齐增量基线到当前硬件计数，但不改变累计位置。
 * @param encoder_h 编码器的 encoder_base 视图；非 NULL，且必须来自已初始化的 encoder_tim_t 首位成员。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化，且不与 GetDelta 或其他基线写操作并发。
 * @post 成功时 last_count 更新为当前原始计数，has_base_count 为 true，overflow 被清除，position 保持不变。
 * @post Platform_Encoder_GetCounter 失败时仅锁存 overflow 为 true，旧 last_count 与 position 保持不变。
 * @note 本函数不返回状态，也不验证当前运动速度；其用途是丢弃上次读取至当前的增量并建立新速度基线。
 * @warning 无锁且非 ISR 安全；与 GetDelta 并发会竞争 last_count，导致下一次增量范围错误。
 */
static void dev_encoder_tim_resync(encoder_base_t *encoder_h) {
  assert(encoder_h);
  encoder_tim_t *encoder_tim_h = (encoder_tim_t*)encoder_h;
  uint32_t counter = 0u;
  if (Platform_Encoder_GetCounter(encoder_tim_h->tim, &counter) != PLATFORM_STATUS_OK) {
    encoder_tim_h->overflow = true;
    return;
  }
  encoder_tim_h->last_count = counter;
  encoder_tim_h->has_base_count = true;
  encoder_tim_h->overflow = false;
}

static const encoder_ops_t encoder_ops_tim = {
  .get_position = dev_encoder_tim_getposition,
  .set_zero = dev_encoder_tim_setzero,
  .get_delta = dev_encoder_tim_getdelta,
  .resync = dev_encoder_tim_resync,
};

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
) {
  assert(encoder_h && name);

  encoder_h->tim = tim;
  platform_status_t status = Platform_Encoder_GetCounterBits(
    encoder_h->tim,
    &encoder_h->bit_width
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  assert(encoder_h->bit_width == 16U || encoder_h->bit_width == 32U);

  Dev_Encoder_Base_Init(&encoder_h->encoder_base, name, &encoder_ops_tim);

  encoder_h->position = 0;
  encoder_h->last_count = 0;
  encoder_h->has_base_count = false;
  encoder_h->overflow = false;
  encoder_h->max_counts_per_period = 0u;

  return Platform_Encoder_Start(encoder_h->tim);
}

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
void Dev_Encoder_TIM_SetMaxCountsPerPeriod(encoder_tim_t *encoder_h, uint32_t max_counts) {
  assert(encoder_h);
  encoder_h->max_counts_per_period = max_counts;
}

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
bool Dev_Encoder_TIM_HasOverflow(const encoder_tim_t *encoder_h) {
  assert(encoder_h);
  return encoder_h->overflow;
}

/**
 * @brief 清除编码器锁存的超限或 Platform 操作错误标志。
 * @param encoder_h 已初始化的编码器对象；非 NULL。
 * @pre encoder_h 已由 Dev_Encoder_TIM_Init 初始化，且与读取操作在同一上下文串行执行。
 * @post overflow 被置为 false；position、last_count、has_base_count 和硬件计数器均不变。
 * @note 本函数不修复底层故障；若 Platform 读写仍失败，下一次 GetDelta/Resync/SetZero 会再次锁存 overflow。
 * @note 不分配内存、不访问硬件，也不清除 Platform 自身的最后错误快照。
 * @warning 无锁且非 ISR 安全；读取方可能观察到清除前后的混合状态，需由调用方保证互斥。
 */
void Dev_Encoder_TIM_ClearOverflow(encoder_tim_t *encoder_h) {
  assert(encoder_h);
  encoder_h->overflow = false;
}
