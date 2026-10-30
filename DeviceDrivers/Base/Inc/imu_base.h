//
// 由 Buns_ 创建于 2026/9/1。
//

/**
 * @file imu_base.h
 * @brief IMU 抽象：原始帧提交、滤波调度、滤波结果与运行状态。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_IMU_BASE_H
#define LAYERED_EMBEDDED_C_IMU_BASE_H

#include "filter_base.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct imu_base_s imu_base_t;

/**
 * @brief IMU 通道位掩码。
 * @note 加速度单位 m/s^2，角速度单位 deg/s，姿态单位 deg。
 */
typedef enum imu_channel_e {
  IMU_CHANNEL_YAW     = 1u << 0,
  IMU_CHANNEL_ROLL    = 1u << 1,
  IMU_CHANNEL_PITCH   = 1u << 2,
  IMU_CHANNEL_GYRO_X  = 1u << 3,
  IMU_CHANNEL_GYRO_Y  = 1u << 4,
  IMU_CHANNEL_GYRO_Z  = 1u << 5,
  IMU_CHANNEL_ACCEL_X = 1u << 6,
  IMU_CHANNEL_ACCEL_Y = 1u << 7,
  IMU_CHANNEL_ACCEL_Z = 1u << 8,
} imu_channel_t;

/**
 * @brief IMU 通道数组下标，与通道位号一致。
 */
typedef enum imu_channel_index_e {
  IMU_CHANNEL_INDEX_YAW = 0u,
  IMU_CHANNEL_INDEX_ROLL,
  IMU_CHANNEL_INDEX_PITCH,
  IMU_CHANNEL_INDEX_GYRO_X,
  IMU_CHANNEL_INDEX_GYRO_Y,
  IMU_CHANNEL_INDEX_GYRO_Z,
  IMU_CHANNEL_INDEX_ACCEL_X,
  IMU_CHANNEL_INDEX_ACCEL_Y,
  IMU_CHANNEL_INDEX_ACCEL_Z,
  IMU_CHANNEL_INDEX_COUNT,
} imu_channel_index_t;

#define IMU_CHANNEL_MASK \
  ((uint32_t)((1u << IMU_CHANNEL_INDEX_COUNT) - 1u))

_Static_assert(
  IMU_CHANNEL_INDEX_COUNT <= FILTER_CHANNEL_CAPACITY,
  "IMU channels must fit the filter_h sample capacity"
);
_Static_assert(
  IMU_CHANNEL_YAW == (1u << IMU_CHANNEL_INDEX_YAW) &&
  IMU_CHANNEL_ROLL == (1u << IMU_CHANNEL_INDEX_ROLL) &&
  IMU_CHANNEL_PITCH == (1u << IMU_CHANNEL_INDEX_PITCH) &&
  IMU_CHANNEL_GYRO_X == (1u << IMU_CHANNEL_INDEX_GYRO_X) &&
  IMU_CHANNEL_GYRO_Y == (1u << IMU_CHANNEL_INDEX_GYRO_Y) &&
  IMU_CHANNEL_GYRO_Z == (1u << IMU_CHANNEL_INDEX_GYRO_Z) &&
  IMU_CHANNEL_ACCEL_X == (1u << IMU_CHANNEL_INDEX_ACCEL_X) &&
  IMU_CHANNEL_ACCEL_Y == (1u << IMU_CHANNEL_INDEX_ACCEL_Y) &&
  IMU_CHANNEL_ACCEL_Z == (1u << IMU_CHANNEL_INDEX_ACCEL_Z),
  "IMU channel masks and indices must match"
);

/**
 * @brief 一次完整 IMU 数据快照。
 * @note 未置位的 valid_mask 通道对应值没有意义。
 * @note channel_update_ms 用于异步通道的新鲜度判断。
 */
typedef struct imu_sample_s {
  float    accel_mps2[3];
  float    gyro_dps[3];
  float    yaw_deg;
  float    roll_deg;
  float    pitch_deg;
  uint32_t valid_mask;
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
  uint32_t sample_time_ms;
  uint32_t sequence;
} imu_sample_t;

/**
 * @brief IMU 运行状态快照。
 * @note valid_mask、时间戳和 sample_count 由 imu_base 统一维护。
 */
typedef struct imu_status_s {
  bool     is_started;
  bool     is_busy;
  bool     has_timestamp;
  bool     last_request_failed;
  uint32_t valid_mask;
  uint32_t sample_count;
  uint32_t error_count;
  uint32_t last_update_ms;
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT];
} imu_status_t;

/**
 * @brief IMU 具体实现的操作表。
 * @note process 在任务上下文运行；set_zero 只处理具体设备侧动作。
 */
typedef struct imu_ops_s {
  void (*start)      (imu_base_t *imu_h);
  void (*process)    (imu_base_t *imu_h, uint32_t now_ms);
  void (*get_status) (imu_base_t *imu_h, imu_status_t *status);
  void (*set_zero)   (imu_base_t *imu_h);
} imu_ops_t;

/**
 * @brief IMU 抽象基类与统一滤波管线。
 * @note filter_h 是非拥有指针，生命周期必须覆盖 IMU 实例。
 * @note filter_h 的输出通道沿用 imu_channel_t 编号。
 */
typedef struct imu_base_s {
  const imu_ops_t *ops;
  const char      *name;
  filter_base_t   *filter_h;

  imu_sample_t raw_sample;
  imu_sample_t sample;
  bool         is_started;
  uint32_t     sample_count;
  float        yaw_offset_deg;
} imu_base_t;

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
);

/**
 * @brief 复位统一滤波器并请求具体 IMU 启动。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @pre imu_h 尚未启动，filter 与 start 操作可用；所有配置和回调已完成绑定。
 * @post 先复位滤波器并调用设备 start，再把 is_started 置为 true。
 * @note 通常在传感器任务初始化阶段调用；本层不加锁且只应调用一次。
 * @warning 设备启动失败无错误码，is_started 仍会置位；重复启动由 assert 捕获。
 */
void Dev_IMU_Base_Start(imu_base_t *imu_h);

/**
 * @brief 推进具体 IMU 的非阻塞采样状态机。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param now_ms 当前单调毫秒时间戳；须与 SubmitSample 使用同一时基。
 * @pre IMU 已启动且绑定了非空 process 操作。
 * @post 具体设备可执行采样并提交帧；基类状态仅在 SubmitSample 成功后更新。
 * @note 由单一传感器任务按周期调用；本层不加锁，禁止与 SubmitSample 并发写入。
 * @warning 不校验时间戳单调性，回退的 now_ms 可能污染具体设备的新鲜度判断。
 */
void Dev_IMU_Base_Process(imu_base_t *imu_h, uint32_t now_ms);

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
);

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
bool Dev_IMU_Base_GetSample(imu_base_t *imu_h, imu_sample_t *sample);

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
bool Dev_IMU_Base_GetRawSample(imu_base_t *imu_h, imu_sample_t *sample);

/**
 * @brief 合并具体设备状态与统一管线状态。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @param status 调用方持有的输出状态；不可为 NULL，函数覆盖其全部字段。
 * @pre 已绑定非空 get_status 操作。
 * @post 设备提供忙碌、请求失败和错误计数，基类覆盖启动、有效掩码、样本计数及时间字段。
 * @note 设备查询在临界区外执行，快照覆盖在临界区内完成；时间单位为 ms，计数单位为样本或错误事件。
 * @warning 输出是组合快照，各字段不保证来自同一精确时刻；本层不保留任何指针。
 */
void Dev_IMU_Base_GetStatus(imu_base_t *imu_h, imu_status_t *status);

/**
 * @brief 读取滤波后的 yaw。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return 相对当前零参考的 yaw，单位 deg，通常归一化到 [-180, 180]。
 * @pre imu_h 已初始化；yaw 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器、偏移或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角区分。
 */
float Dev_IMU_Base_GetYaw(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 roll。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return roll 角，单位 deg；取值范围由已注册滤波器定义。
 * @pre imu_h 已初始化；roll 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角区分。
 */
float Dev_IMU_Base_GetRoll(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 pitch。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return pitch 角，单位 deg；取值范围由已注册滤波器定义。
 * @pre imu_h 已初始化；pitch 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角区分。
 */
float Dev_IMU_Base_GetPitch(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 X 轴角速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return X 轴角速度，单位 deg/s；正负号由车体轴定义。
 * @pre imu_h 已初始化；GX 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角速度区分。
 */
float Dev_IMU_Base_GetGyroX(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 Y 轴角速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Y 轴角速度，单位 deg/s；正负号由车体轴定义。
 * @pre imu_h 已初始化；GY 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角速度区分。
 */
float Dev_IMU_Base_GetGyroY(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 Z 轴角速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Z 轴角速度，单位 deg/s；正负号由车体轴定义。
 * @pre imu_h 已初始化；GZ 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零角速度区分。
 */
float Dev_IMU_Base_GetGyroZ(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 X 轴加速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return X 轴加速度，单位 m/s^2；正负号由车体轴定义。
 * @pre imu_h 已初始化；AX 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零加速度区分。
 */
float Dev_IMU_Base_GetAccelX(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 Y 轴加速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Y 轴加速度，单位 m/s^2；正负号由车体轴定义。
 * @pre imu_h 已初始化；AY 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零加速度区分。
 */
float Dev_IMU_Base_GetAccelY(imu_base_t *imu_h);
/**
 * @brief 读取滤波后的 Z 轴加速度。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @return Z 轴加速度，单位 m/s^2；正负号由车体轴定义。
 * @pre imu_h 已初始化；AZ 通道是否有效由有效快照另行判断。
 * @post 不改变滤波器或原始样本；读取通过快照临界区保护。
 * @warning 无有效样本时返回 0.0f，无法与真实零加速度区分。
 */
float Dev_IMU_Base_GetAccelZ(imu_base_t *imu_h);

/**
 * @brief 把当前发布 yaw 设为零参考。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @pre imu_h 已初始化；仅当发布快照的 YAW 有效位已置位时执行偏移更新。
 * @post yaw 偏移累加当前 yaw 并归一化，发布 yaw 置为 0；raw_sample 与滤波器内部状态不变。
 * @note 操作在 System 临界区内完成；后续发布帧相对于新参考输出。
 * @warning 无有效 yaw 时调用无效果；偏移为 deg，不提供错误码。
 */
void Dev_IMU_Base_ZeroYaw(imu_base_t *imu_h);

/**
 * @brief 请求具体 IMU 执行设备侧置零。
 * @param imu_h 已初始化的 IMU 句柄；不可为 NULL，所有权不转移。
 * @pre 已绑定非空 set_zero 操作；具体设备已处于允许置零的状态。
 * @post 仅委托设备动作，本层不修改 sample、raw_sample、偏移或启动状态。
 * @note 通常在任务上下文调用；是否阻塞、失败恢复和设备侧影响由具体实现决定。
 * @warning 与 Process/SubmitSample 的并发安全由具体设备保证，调用方通常须串行化传感器写路径。
 */
void Dev_IMU_Base_SetZero(imu_base_t *imu_h);

/** @} */

#endif //LAYERED_EMBEDDED_C_IMU_BASE_H
