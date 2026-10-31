//
// 由 Buns_ 创建于 2026/9/1。
//

/**
 * @file imu_base.c
 * @brief IMU 抽象与统一滤波管线实现。
 * @ingroup drivers_base
 */

#include "imu_base.h"
#include "system_angle.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

/**
 * @brief 按通道数组下标定位 IMU 样本中的浮点字段。
 * @param sample 调用方持有的可写样本；不可为 NULL，返回指针借用其存储。
 * @param channel 通道数组下标，范围 0..IMU_CHANNEL_INDEX_COUNT-1；不是位掩码。
 * @return 有效下标返回对应 float 字段地址；assert 关闭时未知下标返回 NULL。
 * @pre sample 非空且 channel 小于 IMU_CHANNEL_INDEX_COUNT。
 * @post 不修改样本内容，不分配内存。
 * @note 私有辅助函数；返回值不是拥有型指针，样本移动、失效或并发写入后不可继续使用。
 * @warning 调用方不得保存返回指针，也不得在无同步的情况下跨任务读写。
 */
static float *imu_value_slot(imu_sample_t *sample, uint32_t channel) {
  assert(sample && channel < IMU_CHANNEL_INDEX_COUNT);

  switch (channel) {
    case IMU_CHANNEL_INDEX_YAW:     return &sample->yaw_deg;
    case IMU_CHANNEL_INDEX_ROLL:    return &sample->roll_deg;
    case IMU_CHANNEL_INDEX_PITCH:   return &sample->pitch_deg;
    case IMU_CHANNEL_INDEX_GYRO_X:  return &sample->gyro_dps[0];
    case IMU_CHANNEL_INDEX_GYRO_Y:  return &sample->gyro_dps[1];
    case IMU_CHANNEL_INDEX_GYRO_Z:  return &sample->gyro_dps[2];
    case IMU_CHANNEL_INDEX_ACCEL_X: return &sample->accel_mps2[0];
    case IMU_CHANNEL_INDEX_ACCEL_Y: return &sample->accel_mps2[1];
    case IMU_CHANNEL_INDEX_ACCEL_Z: return &sample->accel_mps2[2];
    default:                        return NULL;
  }
}

/**
 * @brief 复制滤波帧并清除非有限通道的有效位。
 * @param input 只读源帧；不可为 NULL，valid_mask 只可包含 0..8 位。
 * @param sanitized 调用方持有的副本输出；不可为 NULL，函数覆盖其全部内容。
 * @retval true 清洗后至少保留一个有效通道。
 * @retval false 所有已置位通道均为非有限值。
 * @pre input->valid_mask 不包含 IMU_CHANNEL_MASK 之外的位。
 * @post sanitized 是 input 的完整副本，仅清除非有限有效通道的位；input 不变。
 * @note 私有辅助函数，不分配内存；检查范围为 IMU 的 0..8 通道。
 * @warning 只有当前帧内非有限值会被移除，不会保留或修复前一帧数据。
 */
static bool imu_sanitize_frame(
  const filter_sample_t *input,
  filter_sample_t *sanitized
) {
  assert(input && sanitized);
  assert((input->valid_mask & ~IMU_CHANNEL_MASK) == 0u);

  *sanitized = *input;

  for (
    uint32_t channel = 0u;
    channel < IMU_CHANNEL_INDEX_COUNT;
    channel++
  ) {
    uint32_t mask = (uint32_t)1u << channel;
    if (
      (sanitized->valid_mask & mask) != 0u &&
      !SYS_ISFINITE(sanitized->value[channel])
    ) {
      sanitized->valid_mask &= ~mask;
    }
  }

  return sanitized->valid_mask != 0u;
}

/**
 * @brief 把一帧滤波结果合并到 IMU 快照。
 * @param sample 调用方持有的可写目标快照；不可为 NULL，函数原地更新。
 * @param frame 只读滤波帧；不可为 NULL，仅 0..8 位参与合并，更高位被忽略。
 * @param clear_mask IMU 通道位掩码；置位且帧中缺失的通道会从目标清除。
 * @param yaw_offset_deg yaw 零参考偏移，单位 deg；必须有限。
 * @retval true 至少一个有限通道被更新。
 * @retval false 没有通道被更新。
 * @pre sample/frame 非空，yaw_offset_deg 有限；sample 的通道位不超出 IMU_CHANNEL_MASK。
 * @post 有效通道写入值、有效位和帧时间；缺失且命中 clear_mask 的通道被清除；有更新时 sequence 只递增一次。
 * @note 非有限帧通道会清除目标对应有效位而不发布；当前调用方在临界区内使用本函数。
 * @warning 本函数不获取锁，必须由调用方保证 sample 的并发独占写入。
 */
static bool imu_merge_frame(
  imu_sample_t *sample,
  const filter_sample_t *frame,
  uint32_t clear_mask,
  float yaw_offset_deg
) {
  assert(sample && frame);

  bool updated = false;

  for (
    uint32_t channel = 0u;
    channel < IMU_CHANNEL_INDEX_COUNT;
    channel++
  ) {
    uint32_t mask = (uint32_t)1u << channel;

    if (
      (clear_mask & mask) != 0u &&
      (frame->valid_mask & mask) == 0u
    ) {
      sample->valid_mask &= ~mask;
    }

    if ((frame->valid_mask & mask) == 0u) {
      continue;
    }

    float value = frame->value[channel];
    if (!SYS_ISFINITE(value)) {
      sample->valid_mask &= ~mask;
      continue;
    }

    if (channel == IMU_CHANNEL_INDEX_YAW) {
      float wrapped_deg = 0.0f;
      if (!Sys_Angle_WrapDegrees(value - yaw_offset_deg, &wrapped_deg)) {
        sample->valid_mask &= ~mask;
        continue;
      }
      value = wrapped_deg;
    }

    float *value_slot = imu_value_slot(sample, channel);
    assert(value_slot != NULL);

    *value_slot = value;
    sample->valid_mask |= mask;
    sample->channel_update_ms[channel] = frame->time_ms;
    sample->sample_time_ms = frame->time_ms;
    updated = true;
  }

  if (updated) {
    sample->sequence++;
  }

  return updated;
}

/**
 * @brief 绑定 IMU 设备、操作表与统一滤波器。
 * @param imu_h 调用方持有的 IMU 基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖 IMU 对象；不可为 NULL。
 * @param ops 具体设备操作表；start、process、get_status、set_zero 均必须非空。
 * @param filter_h 已初始化的滤波器根节点；非拥有，生命周期必须覆盖 IMU。
 * @pre imu_h 尚未并发发布，name、ops、filter_h 在对象寿命内保持有效。
 * @post 整个基类清零后保存绑定；保持未启动、无有效样本、无 yaw 偏移。
 * @note 仅限初始化阶段调用；不启动硬件、不复位滤波器、不加锁、不分配内存。
 */
void Dev_IMU_Base_Init(
  imu_base_t *imu_h,
  const char *name,
  const imu_ops_t *ops,
  filter_base_t *filter_h
) {
  assert(
    imu_h         &&
    name         &&
    ops          &&
    filter_h       &&
    ops->start   &&
    ops->process &&
    ops->get_status &&
    ops->set_zero
  );

  memset(imu_h, 0, sizeof(*imu_h));
  imu_h->ops    = ops;
  imu_h->name   = name;
  imu_h->filter_h = filter_h;
}

/**
 * @brief 复位统一滤波器并请求具体 IMU 启动。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @pre imu_h 尚未启动，filter 与 start 操作可用；所有配置和回调已完成绑定。
 * @post 先复位滤波器并调用设备 start，再把 is_started 置为 true。
 * @note 通常在传感器任务初始化阶段调用；本层不加锁且只应调用一次。
 * @warning 设备启动失败无错误码，is_started 仍会置位；重复启动由 assert 捕获。
 */
void Dev_IMU_Base_Start(imu_base_t *imu_h) {
  assert(imu_h && imu_h->ops && imu_h->filter_h && !imu_h->is_started);

  Filter_Base_Reset(imu_h->filter_h);
  imu_h->ops->start(imu_h);
  imu_h->is_started = true;
}

/**
 * @brief 推进具体 IMU 的非阻塞采样状态机。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param now_ms 当前单调毫秒时间戳；须与 SubmitSample 使用同一时基。
 * @pre IMU 已启动且绑定了非空 process 操作。
 * @post 具体设备可执行采样并提交帧；基类状态仅在 SubmitSample 成功后更新。
 * @note 由单一传感器任务按周期调用；本层不加锁，禁止与 SubmitSample 并发写入。
 * @warning 不校验时间戳单调性，回退的 now_ms 可能污染具体设备的新鲜度判断。
 */
void Dev_IMU_Base_Process(imu_base_t *imu_h, uint32_t now_ms) {
  assert(imu_h && imu_h->ops && imu_h->ops->process && imu_h->is_started);
  imu_h->ops->process(imu_h, now_ms);
}

/**
 * @brief 清洗一帧原始数据并运行统一滤波器。
 * @param imu_h 已启动的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param frame 只读原始帧；valid_mask 只可置位通道 0..8，时间为单调毫秒。
 * @retval true 滤波器至少发布了一个有限通道，sample_count 已递增。
 * @retval false 输入没有有效有限通道，或滤波输出没有可发布通道。
 * @pre IMU 已启动且 filter_h 有效；frame 中的位号 0..8 分别为姿态 deg、角速度 deg/s、加速度 m/s^2。
 * @post 原始有效通道写入 raw_sample；滤波结果按掩码合并到 sample，非有限值不发布。
 * @note 输入缓冲不保留、所有权不转移；临界区只保护快照合并，滤波本身在临界区外执行。
 * @warning Process 与本函数只允许一个写者；读取者可并发使用 GetSample/GetRawSample/GetStatus。
 */
bool Dev_IMU_Base_SubmitSample(
  imu_base_t *imu_h,
  const filter_sample_t *frame
) {
  assert(
    imu_h           &&
    frame          &&
    imu_h->filter_h   &&
    imu_h->is_started
  );

  filter_sample_t sanitized;
  if (!imu_sanitize_frame(frame, &sanitized)) {
    return false;
  }

  Sys_Critical_Enter();
  (void)imu_merge_frame(&imu_h->raw_sample, &sanitized, 0u, 0.0f);
  Sys_Critical_Exit();

  filter_sample_t filtered;
  Filter_Base_Update(imu_h->filter_h, &sanitized, &filtered);

  Sys_Critical_Enter();

  bool published = imu_merge_frame(
    &imu_h->sample,
    &filtered,
    sanitized.valid_mask,
    imu_h->yaw_offset_deg
  );

  if (published) {
    imu_h->sample_count++;
  }

  Sys_Critical_Exit();

  return published;
}

/**
 * @brief 复制最近一次发布的滤波 IMU 快照。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param sample 调用方持有的输出快照；不可为 NULL，函数始终覆盖其全部内容。
 * @retval true IMU 已启动且至少发布过一个滤波样本。
 * @retval false 尚未启动或尚无发布样本；sample 仍含当前零值或历史快照。
 * @pre imu_h 已初始化，sample 在调用期间可写。
 * @post 输出与写者通过 System 临界区一致复制；滤波器与原始样本不被修改。
 * @note 快照中的有效通道由 valid_mask 指示；未置位通道值无意义。
 */
bool Dev_IMU_Base_GetSample(imu_base_t *imu_h, imu_sample_t *sample) {
  assert(imu_h && sample);

  Sys_Critical_Enter();
  *sample = imu_h->sample;
  bool available = imu_h->is_started && sample->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

/**
 * @brief 复制最近一次清洗后的原始 IMU 快照。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param sample 调用方持有的输出快照；不可为 NULL，函数始终覆盖其全部内容。
 * @retval true IMU 已启动且至少提交过一个有效原始帧。
 * @retval false 尚未启动或尚无有效原始通道；sample 仍含当前零值或历史快照。
 * @pre imu_h 已初始化，sample 在调用期间可写。
 * @post 输出与写者通过 System 临界区一致复制；不运行滤波器、不改变样本。
 * @note 该快照不含 yaw 偏移，也未应用滤波器的校正或通道增删。
 */
bool Dev_IMU_Base_GetRawSample(imu_base_t *imu_h, imu_sample_t *sample) {
  assert(imu_h && sample);

  Sys_Critical_Enter();
  *sample = imu_h->raw_sample;
  bool available = imu_h->is_started && sample->sequence != 0u;
  Sys_Critical_Exit();

  return available;
}

/**
 * @brief 合并具体设备状态与统一管线状态。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param status 调用方持有的输出状态；不可为 NULL，函数覆盖其全部字段。
 * @pre 已绑定非空 get_status 操作。
 * @post 设备提供忙碌、请求失败和错误计数，基类覆盖启动、有效掩码、样本计数及时间字段。
 * @note 设备查询在临界区外执行，快照覆盖在临界区内完成；时间单位为 ms，计数单位为样本或错误事件。
 * @warning 输出是组合快照，各字段不保证来自同一精确时刻；本层不保留任何指针。
 */
void Dev_IMU_Base_GetStatus(imu_base_t *imu_h, imu_status_t *status) {
  assert(imu_h && imu_h->ops && imu_h->ops->get_status && status);

  imu_status_t device_status = {0};
  imu_h->ops->get_status(imu_h, &device_status);

  Sys_Critical_Enter();
  device_status.is_started = imu_h->is_started;
  device_status.has_timestamp = imu_h->sample.sequence != 0u;
  device_status.valid_mask = imu_h->sample.valid_mask;
  device_status.sample_count = imu_h->sample_count;
  device_status.last_update_ms = imu_h->sample.sample_time_ms;

  memcpy(
    device_status.channel_update_ms,
    imu_h->sample.channel_update_ms,
    sizeof(device_status.channel_update_ms)
  );

  Sys_Critical_Exit();

  *status = device_status;
}

/**
 * @brief 读取滤波后的 yaw。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return 相对当前零参考的 yaw，单位 deg，通常归一化到 [-180, 180]。
 * @pre imu_h 已初始化；yaw 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器、偏移或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角区分。
 */
float Dev_IMU_Base_GetYaw(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.yaw_deg;
}

/**
 * @brief 读取滤波后的 roll。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return roll 角，单位 deg；取值范围由已注册滤波器定义。
 * @pre imu_h 已初始化；roll 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角区分。
 */
float Dev_IMU_Base_GetRoll(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.roll_deg;
}

/**
 * @brief 读取滤波后的 pitch。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return pitch 角，单位 deg；取值范围由已注册滤波器定义。
 * @pre imu_h 已初始化；pitch 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角区分。
 */
float Dev_IMU_Base_GetPitch(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.pitch_deg;
}

/**
 * @brief 读取滤波后的 X 轴角速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return X 轴角速度，单位 deg/s；正负号由车体轴定义。
 * @pre imu_h 已初始化；GX 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角速度区分。
 */
float Dev_IMU_Base_GetGyroX(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[0];
}

/**
 * @brief 读取滤波后的 Y 轴角速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Y 轴角速度，单位 deg/s；正负号由车体轴定义。
 * @pre imu_h 已初始化；GY 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角速度区分。
 */
float Dev_IMU_Base_GetGyroY(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[1];
}

/**
 * @brief 读取滤波后的 Z 轴角速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Z 轴角速度，单位 deg/s；正负号由车体轴定义。
 * @pre imu_h 已初始化；GZ 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角速度区分。
 */
float Dev_IMU_Base_GetGyroZ(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.gyro_dps[2];
}

/**
 * @brief 读取滤波后的 X 轴加速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return X 轴加速度，单位 m/s^2；正负号由车体轴定义。
 * @pre imu_h 已初始化；AX 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零加速度区分。
 */
float Dev_IMU_Base_GetAccelX(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[0];
}

/**
 * @brief 读取滤波后的 Y 轴加速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Y 轴加速度，单位 m/s^2；正负号由车体轴定义。
 * @pre imu_h 已初始化；AY 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零加速度区分。
 */
float Dev_IMU_Base_GetAccelY(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[1];
}

/**
 * @brief 读取滤波后的 Z 轴加速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Z 轴加速度，单位 m/s^2；正负号由车体轴定义。
 * @pre imu_h 已初始化；AZ 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零加速度区分。
 */
float Dev_IMU_Base_GetAccelZ(imu_base_t *imu_h) {
  imu_sample_t sample = {0};
  (void)Dev_IMU_Base_GetSample(imu_h, &sample);
  return sample.accel_mps2[2];
}

/**
 * @brief 把当前发布 yaw 设为零参考。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @pre imu_h 已初始化；仅当发布快照的 YAW 有效位已置位时执行偏移更新。
 * @post yaw 偏移累加当前 yaw 并归一化，发布 yaw 置为 0；raw_sample 与滤波器内部状态不变。
 * @note 操作在 System 临界区内完成；后续发布帧相对于新参考输出。
 * @warning 无有效 yaw 时调用无效果；偏移为 deg，不提供错误码。
 */
void Dev_IMU_Base_ZeroYaw(imu_base_t *imu_h) {
  assert(imu_h);

  Sys_Critical_Enter();

  if ((imu_h->sample.valid_mask & IMU_CHANNEL_YAW) != 0u) {
    float wrapped_deg = 0.0f;
    if (Sys_Angle_WrapDegrees(
          imu_h->yaw_offset_deg + imu_h->sample.yaw_deg,
          &wrapped_deg
        )) {
      imu_h->yaw_offset_deg = wrapped_deg;
      imu_h->sample.yaw_deg = 0.0f;
    }
  }

  Sys_Critical_Exit();
}

/**
 * @brief 请求具体 IMU 执行设备侧置零。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @pre 已绑定非空 set_zero 操作；具体设备已处于允许置零的状态。
 * @post 仅委托设备动作，本层不修改 sample、raw_sample、偏移或启动状态。
 * @note 通常在任务上下文调用；是否阻塞、失败恢复和设备侧影响由具体实现决定。
 * @warning 与 Process/SubmitSample 的并发安全由具体设备保证，调用方通常须串行化传感器写路径。
 */
void Dev_IMU_Base_SetZero(imu_base_t *imu_h) {
  assert(imu_h && imu_h->ops && imu_h->ops->set_zero);
  imu_h->ops->set_zero(imu_h);
}
