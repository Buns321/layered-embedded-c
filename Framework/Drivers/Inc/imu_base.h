/**
 * @file imu_base.h
 * @brief 9 通道 IMU 多态基类、统一滤波管线与线程安全快照接口。
 * @details 属于 Framework/Drivers 设备驱动层，为串口陀螺仪（WHT101）与 I2C 六轴
 *          （MPU6050）等具体 IMU 提供统一的 9 通道数据模型、filter_base_t 滤波管线、
 *          软/硬偏航归零以及临界区保护的原子快照发布/读取接口。
 *          所有对象均采用静态分配，禁止按值拷贝基类。
 *
 * @defgroup Framework_Drivers Framework 设备驱动层
 * @{
 */

#ifndef LAYERED_CAR_IMU_BASE_H
#define LAYERED_CAR_IMU_BASE_H

#include "filter_base.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief IMU 多态基类前向声明。 */
typedef struct imu_base_s imu_base_t;

/**
 * @brief IMU 通道位掩码枚举。
 * @details 物理单位约定：加速度为 m/s^2，角速度为 deg/s，姿态角为 deg。
 */
typedef enum imu_channel_e {
  IMU_CHANNEL_YAW = 1U << 0U,     /**< 偏航角通道掩码（单位：deg） */
  IMU_CHANNEL_ROLL = 1U << 1U,    /**< 横滚角通道掩码（单位：deg） */
  IMU_CHANNEL_PITCH = 1U << 2U,   /**< 俯仰角通道掩码（单位：deg） */
  IMU_CHANNEL_GYRO_X = 1U << 3U,  /**< X 轴角速度通道掩码（单位：deg/s） */
  IMU_CHANNEL_GYRO_Y = 1U << 4U,  /**< Y 轴角速度通道掩码（单位：deg/s） */
  IMU_CHANNEL_GYRO_Z = 1U << 5U,  /**< Z 轴角速度通道掩码（单位：deg/s） */
  IMU_CHANNEL_ACCEL_X = 1U << 6U, /**< X 轴加速度通道掩码（单位：m/s^2） */
  IMU_CHANNEL_ACCEL_Y = 1U << 7U, /**< Y 轴加速度通道掩码（单位：m/s^2） */
  IMU_CHANNEL_ACCEL_Z = 1U << 8U  /**< Z 轴加速度通道掩码（单位：m/s^2） */
} imu_channel_t;

/**
 * @brief IMU 通道数组下标枚举。
 */
typedef enum imu_channel_index_e {
  IMU_CHANNEL_INDEX_YAW = 0U, /**< 偏航角下标（0） */
  IMU_CHANNEL_INDEX_ROLL,     /**< 横滚角下标（1） */
  IMU_CHANNEL_INDEX_PITCH,    /**< 俯仰角下标（2） */
  IMU_CHANNEL_INDEX_GYRO_X,   /**< X 轴角速度下标（3） */
  IMU_CHANNEL_INDEX_GYRO_Y,   /**< Y 轴角速度下标（4） */
  IMU_CHANNEL_INDEX_GYRO_Z,   /**< Z 轴角速度下标（5） */
  IMU_CHANNEL_INDEX_ACCEL_X,  /**< X 轴加速度下标（6） */
  IMU_CHANNEL_INDEX_ACCEL_Y,  /**< Y 轴加速度下标（7） */
  IMU_CHANNEL_INDEX_ACCEL_Z,  /**< Z 轴加速度下标（8） */
  IMU_CHANNEL_INDEX_COUNT     /**< IMU 通道总数（9） */
} imu_channel_index_t;

/** @brief 全部 9 路有效 IMU 通道的联合位掩码（低 9 位全 1）。 */
#define IMU_CHANNEL_MASK ((uint32_t)((1UL << IMU_CHANNEL_INDEX_COUNT) - 1UL))

_Static_assert(IMU_CHANNEL_INDEX_COUNT <= FILTER_CHANNEL_CAPACITY,
               "IMU channels must fit filter sample capacity");

/**
 * @brief IMU 一致性数据快照结构体。
 */
typedef struct imu_sample_s {
  float accel_mps2[3]; /**< 三轴线加速度 [X, Y, Z]（单位：m/s^2） */
  float gyro_dps[3];   /**< 三轴角速度 [X, Y, Z]（单位：deg/s） */
  float yaw_deg;       /**< 偏航角，逆时针为正（单位：deg，滤波样本已折叠至 [-180, 180)） */
  float roll_deg;      /**< 横滚角（单位：deg） */
  float pitch_deg;     /**< 俯仰角（单位：deg） */
  uint32_t valid_mask; /**< 当前有效通道位掩码（由 imu_channel_t 按位或组成） */
  uint32_t channel_update_ms
      [IMU_CHANNEL_INDEX_COUNT]; /**< 各通道最近一次有效更新的单调时间戳（单位：ms） */
  uint32_t sample_time_ms;       /**< 最近一次任意通道更新的时间戳（单位：ms） */
  uint32_t sequence;             /**< 单调递增快照序号（0 表示尚无有效样本） */
} imu_sample_t;

/**
 * @brief IMU 运行状态快照结构体。
 */
typedef struct imu_status_s {
  bool is_started;          /**< 是否已调用 Dev_IMU_Base_Start() 启动 */
  bool is_busy;             /**< 具体设备是否正处于复位、校准或串口配置忙状态 */
  bool has_timestamp;       /**< 是否已发布过至少一帧有效样本（sequence != 0） */
  bool last_request_failed; /**< 最近一次设备通信或配置请求是否失败 */
  uint32_t valid_mask;      /**< 当前已发布的有效通道位掩码 */
  uint32_t sample_count;    /**< 累计成功发布到滤波快照的次数 */
  uint32_t error_count;     /**< 具体设备上报的累计通信/校验/配置错误计数 */
  uint32_t last_update_ms;  /**< 最近一次有效样本发布时间（单位：ms） */
  uint32_t channel_update_ms[IMU_CHANNEL_INDEX_COUNT]; /**< 各通道最近有效更新时间（单位：ms） */
} imu_status_t;

/**
 * @brief 具体 IMU 设备只读虚表（多态操作接口）。
 */
typedef struct imu_ops_s {
  void (*start)(imu_base_t *imu_h); /**< 启动底层通信或初始化状态机 */
  void (*process)(imu_base_t *imu_h,
                  uint32_t now_ms); /**< 周期推进非阻塞采样与协议解析（now_ms 单位：ms） */
  void (*get_status)(imu_base_t *imu_h, imu_status_t *status); /**< 读取设备特有状态与错误计数 */
  void (*set_zero)(imu_base_t *imu_h);                         /**< 执行硬件或软件偏航角归零 */
} imu_ops_t;

/**
 * @brief IMU 多态基类结构体。
 * @note 必须作为具体 IMU 设备结构体的第一个成员，禁止按值拷贝。
 */
typedef struct imu_base_s {
  const imu_ops_t *ops;    /**< 指向具体设备只读虚表的指针 */
  const char *name;        /**< IMU 实例调试名称字符串 */
  filter_base_t *filter_h; /**< 绑定的 9 通道滤波器句柄 */
  imu_sample_t raw_sample; /**< 清洗后未滤波的原始数据快照（受临界区保护） */
  imu_sample_t sample;     /**< 经滤波器与偏航偏移补偿后的发布快照（受临界区保护） */
  bool is_started;         /**< 基类启动标志 */
  uint32_t sample_count;   /**< 累计发布快照计数 */
  float yaw_offset_deg;    /**< 软件偏航零参考偏移量（单位：deg） */
} imu_base_t;

/**
 * @brief 绑定 IMU 设备虚表与滤波器。
 * @param[out] imu_h    IMU 基类指针（为 NULL 时安全忽略）。
 * @param[in]  name     调试名称字符串（非 NULL）。
 * @param[in]  ops      只读虚表指针（所有回调均须非 NULL）。
 * @param[in]  filter_h 滤波器句柄（非 NULL）。
 * @pre 仅在系统初始化阶段、任务调度启动前调用。
 */
void Dev_IMU_Base_Init(imu_base_t *imu_h, const char *name, const imu_ops_t *ops,
                       filter_base_t *filter_h);

/**
 * @brief 复位滤波器并启动具体 IMU 设备。
 * @param[in,out] imu_h IMU 基类指针（为 NULL、未初始化或已启动时安全忽略）。
 * @pre 须先调用 Dev_IMU_Base_Init()；应在任务上下文调用一次。
 */
void Dev_IMU_Base_Start(imu_base_t *imu_h);

/**
 * @brief 推进 IMU 非阻塞采样与状态机。
 * @param[in,out] imu_h  IMU 基类指针（为 NULL 或未启动时直接返回）。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 须在单一传感器/输入任务中周期性调用。
 */
void Dev_IMU_Base_Process(imu_base_t *imu_h, uint32_t now_ms);

/**
 * @brief 清洗原始帧、运行滤波管线并原子合并发布快照。
 * @param[in,out] imu_h IMU 基类指针（非 NULL 且须已启动）。
 * @param[in]     frame 待提交的多通道样本帧（time_ms 单位：ms）。
 * @return 是否成功发布有效更新。
 * @retval true  至少有一个通道通过有限性校验、滤波并更新至发布快照。
 * @retval false 参数无效、未启动、含越界掩码或所有通道均非有限值。
 * @note 内部通过 Sys_Critical_Enter/Exit() 保护 raw_sample 与 sample 快照一致性。
 */
bool Dev_IMU_Base_SubmitSample(imu_base_t *imu_h, const filter_sample_t *frame);

/**
 * @brief 临界区内复制最近一次滤波后的 IMU 快照。
 * @param[in]  imu_h  IMU 基类指针（非 NULL）。
 * @param[out] sample 接收快照的输出结构体指针（非 NULL）。
 * @return 快照是否有效可用。
 * @retval true  已启动且至少发布过一帧有效样本（sequence != 0）。
 * @retval false 参数为空、未启动或尚无有效样本。
 */
bool Dev_IMU_Base_GetSample(const imu_base_t *imu_h, imu_sample_t *sample);

/**
 * @brief 临界区内复制最近一次清洗后未滤波的原始 IMU 快照。
 * @param[in]  imu_h  IMU 基类指针（非 NULL）。
 * @param[out] sample 接收原始快照的输出结构体指针（非 NULL）。
 * @return 原始快照是否有效可用。
 * @retval true  已启动且至少接收过一帧有效原始样本（sequence != 0）。
 * @retval false 参数为空、未启动或尚无有效样本。
 */
bool Dev_IMU_Base_GetRawSample(const imu_base_t *imu_h, imu_sample_t *sample);

/**
 * @brief 合并具体设备状态与基类管线状态。
 * @param[in,out] imu_h  IMU 基类指针（非 NULL）。
 * @param[out]    status 接收合并状态的输出结构体指针（非 NULL）。
 */
void Dev_IMU_Base_GetStatus(imu_base_t *imu_h, imu_status_t *status);

/**
 * @brief 读取当前相对零参考的偏航角。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return 偏航角（单位：deg，范围 [-180, 180)；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetYaw(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后横滚角。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return 横滚角（单位：deg；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetRoll(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后俯仰角。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return 俯仰角（单位：deg；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetPitch(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后 X 轴角速度。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return X 轴角速度（单位：deg/s；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetGyroX(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后 Y 轴角速度。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return Y 轴角速度（单位：deg/s；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetGyroY(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后 Z 轴角速度。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return Z 轴角速度（单位：deg/s；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetGyroZ(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后 X 轴线加速度。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return X 轴加速度（单位：m/s^2；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetAccelX(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后 Y 轴线加速度。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return Y 轴加速度（单位：m/s^2；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetAccelY(const imu_base_t *imu_h);

/**
 * @brief 读取滤波后 Z 轴线加速度。
 * @param[in] imu_h IMU 基类指针（允许为 NULL）。
 * @return Z 轴加速度（单位：m/s^2；无有效样本时返回 0.0f）。
 */
float Dev_IMU_Base_GetAccelZ(const imu_base_t *imu_h);

/**
 * @brief 软件将当前有效偏航角置为相对零参考。
 * @details 临界区内将当前 sample.yaw_deg 累加至 yaw_offset_deg 并折叠到 [-180, 180)，
 *          同时将当前发布快照的 yaw_deg 立即清零。
 * @param[in,out] imu_h IMU 基类指针（为 NULL 时安全忽略）。
 */
void Dev_IMU_Base_ZeroYaw(imu_base_t *imu_h);

/**
 * @brief 请求具体设备执行硬件或软件置零。
 * @param[in,out] imu_h IMU 基类指针（为 NULL 或虚表无效时安全忽略）。
 */
void Dev_IMU_Base_SetZero(imu_base_t *imu_h);

#endif /* LAYERED_CAR_IMU_BASE_H */

/** @} */
