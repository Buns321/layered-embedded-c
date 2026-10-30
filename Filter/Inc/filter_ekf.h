//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file filter_ekf.h
 * @brief 七状态四元数 IMU EKF 接口与在线参数入口。
 * @addtogroup filter
 * @{
 * @note 状态向量为 [qw, qx, qy, qz, bgx, bgy, bgz]。
 * @note EKF 核心、姿态发布和参数入口使用 SI 单位；Filter 框架边界把
 *       陀螺 deg/s 转为 rad/s，再把姿态 rad 转回 deg。
 * @warning 当前核心预测与校正为占位实现，姿态结果不可用于闭环控制。
 */

#ifndef LAYERED_EMBEDDED_C_FILTER_EKF_H
#define LAYERED_EMBEDDED_C_FILTER_EKF_H

#include "filter_base.h"
#include <stdbool.h>
#include <stdint.h>

#define FILTER_EKF_STATE_DIMENSION 7u

/**
 * @brief EKF 可在线调整参数。
 */
typedef enum filter_ekf_parameter_e {
  FILTER_EKF_PARAM_ANGLE_PROCESS_NOISE = 0U,
  FILTER_EKF_PARAM_BIAS_PROCESS_NOISE,
  FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE,
  FILTER_EKF_PARAM_INITIAL_ANGLE_VARIANCE,
  FILTER_EKF_PARAM_INITIAL_BIAS_VARIANCE,
  FILTER_EKF_PARAM_ACCEL_NORM_MIN,
  FILTER_EKF_PARAM_ACCEL_NORM_MAX,
  FILTER_EKF_PARAM_GYRO_LIMIT,
  FILTER_EKF_PARAM_BIAS_LIMIT,
  FILTER_EKF_PARAM_MIN_DT,
  FILTER_EKF_PARAM_MAX_DT,
  FILTER_EKF_PARAM_COUNT,
} filter_ekf_parameter_t;

/**
 * @brief EKF 运行参数和默认值。
 * @note 噪声参数为方差量纲；dt 和模长门限使用 SI 单位。
 */
typedef struct filter_ekf_config_s {
  /** @brief 姿态过程噪声方差，单位 rad²，必须大于零。 */
  float angle_process_noise_rad2;
  /** @brief 陀螺零偏过程噪声方差，单位 (rad/s)²，必须大于零。 */
  float bias_process_noise_radps2;
  /** @brief 加速度观测噪声方差，单位 (m/s²)²，必须大于零。 */
  float accel_measurement_noise_mps2_2;
  /** @brief 初始姿态方差，单位 rad²，必须大于零。 */
  float initial_angle_variance_rad2;
  /** @brief 初始零偏方差，单位 (rad/s)²，必须大于零。 */
  float initial_bias_variance_radps2;
  /** @brief 加速度模长观测下限，单位 m/s²，必须大于零。 */
  float accel_norm_min_mps2;
  /** @brief 加速度模长观测上限，单位 m/s²，必须大于下限。 */
  float accel_norm_max_mps2;
  /** @brief 陀螺角速度保护上限，单位 rad/s，必须大于零。 */
  float gyro_limit_radps;
  /** @brief 陀螺零偏保护上限，单位 rad/s，必须大于零。 */
  float bias_limit_radps;
  /** @brief 最小融合周期，单位 s，必须大于零。 */
  float min_dt_s;
  /** @brief 最大融合周期，单位 s，必须不小于最小周期。 */
  float max_dt_s;
} filter_ekf_config_t;

/**
 * @brief EKF 对外状态。
 * @note quaternion 顺序固定为 w、x、y、z。
 */
typedef struct filter_ekf_state_s {
  /** @brief 单位四元数，顺序为 w、x、y、z。 */
  float quaternion[4];
  /** @brief 三轴陀螺零偏，单位 rad/s。 */
  float gyro_bias_radps[3];
  /** @brief 横滚角，单位 rad。 */
  float roll_rad;
  /** @brief 俯仰角，单位 rad。 */
  float pitch_rad;
  /** @brief 偏航角，单位 rad。 */
  float yaw_rad;
  /** @brief 最近一次融合周期，单位 s。 */
  float dt_s;
  /** @brief 已完成的核心更新次数。 */
  uint32_t update_count;
  /** @brief 当前姿态是否可用于消费。 */
  bool valid;
  /** @brief true 表示姿态由未实现的占位核心发布。 */
  bool placeholder;
} filter_ekf_state_t;

/**
 * @brief 七状态 EKF 对象。
 * @note filter_base 必须是首成员。
 * @note covariance 的排列顺序与状态向量一致。
 */
typedef struct filter_ekf_s {
  filter_base_t filter_base;
  filter_ekf_config_t config;
  filter_ekf_config_t default_config;
  filter_ekf_config_t pending_config;
  uint32_t pending_mask;
  filter_ekf_state_t state;
  float covariance[FILTER_EKF_STATE_DIMENSION][FILTER_EKF_STATE_DIMENSION];
  uint32_t last_time_ms;
  bool has_time;
} filter_ekf_t;

/**
 * @brief 初始化七状态 EKF。
 * @param filter_h 待初始化的 EKF 存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖 EKF 对象。
 * @param config 非空默认参数；噪声和门限必须为规定的正有限值，max_dt_s 不小于 min_dt_s。
 * @retval true 参数有效，EKF 状态、默认参数、待提交参数和协方差已初始化。
 * @retval false filter_h/name/config 为空或配置无效；filter_h 保持不变。
 * @pre 仅允许在任务初始化阶段单线程调用，且对象未被其他上下文使用。
 * @post update_count 清零、四元数为单位四元数、placeholder 为 true，pending_mask 为 0。
 * @note 当前核心为占位实现，该函数只建立接口和参数状态；不分配动态内存。
 */
bool Filter_Ekf_Init(
  filter_ekf_t *filter_h,
  const char *name,
  const filter_ekf_config_t *config
);

/**
 * @brief 提交一个在线参数。
 * @param filter_h 已初始化的 EKF，不能为 NULL。
 * @param parameter 参数身份，范围 0..FILTER_EKF_PARAM_COUNT-1。
 * @param value 新值；单位随 parameter：ANGLE_PROCESS_NOISE/INITIAL_ANGLE_VARIANCE 为 rad²，
 *              BIAS_PROCESS_NOISE/INITIAL_BIAS_VARIANCE 为 (rad/s)²，
 *              ACCEL_MEASUREMENT_NOISE 为 (m/s²)²，ACCEL_NORM_MIN/MAX 为 m/s²，
 *              GYRO_LIMIT/BIAS_LIMIT 为 rad/s，MIN_DT/MAX_DT 为 s。
 * @retval true 候选配置整体有效，已合并进待提交配置。
 * @retval false filter_h 为空、参数身份非法、value 非有限，或候选配置未通过组合范围校验。
 * @pre 单参数写者调用；value 必须与 parameter 的物理量一致，函数不进行单位换算。
 * @post 成功时待提交配置更新且 pending_mask 置位，下一帧开始时一次性应用；失败时活动配置和已有待提交值不变。
 * @note 与 update 的配置应用通过临界区交换快照；不修改正在使用的滤波状态或协方差。
 */
bool Filter_Ekf_SetParameter(
  filter_ekf_t *filter_h,
  filter_ekf_parameter_t parameter,
  float value
);

/**
 * @brief 请求把所有参数恢复为初始化时的默认值。
 * @param filter_h 已初始化的 EKF，不能为 NULL。
 * @pre 单参数写者调用；default_config 已由 Filter_Ekf_Init 保存。
 * @post pending_config 被替换为 default_config，pending_mask 标记全部参数。
 * @note 新参数在下一帧开始时原子应用；不立即改变 active config 或协方差，不分配动态内存。
 */
void Filter_Ekf_ResetParameters(filter_ekf_t *filter_h);

/**
 * @brief 复制当前 EKF 参数。
 * @param filter_h 已初始化的 EKF，不能为 NULL。
 * @param config 调用方拥有的输出存储，不能为 NULL。
 * @pre config 可写且不与内部 config 重叠。
 * @post config 获得 active config 的完整一致快照；pending 参数不会混入。
 * @note 可从其他任务读取；不分配动态内存，不改变 EKF 状态。
 */
void Filter_Ekf_GetConfig(
  const filter_ekf_t *filter_h,
  filter_ekf_config_t *config
);

/**
 * @brief 复制最近一次 EKF 状态。
 * @param filter_h 已初始化的 EKF，不能为 NULL。
 * @param state 调用方拥有的输出存储，不能为 NULL。
 * @pre state 可写且不与内部状态重叠。
 * @post state 获得姿态、零偏、dt_s、更新计数和 valid/placeholder 的一致快照。
 * @note 当前 placeholder 为 true 时姿态不可用于闭环控制；不分配动态内存。
 */
void Filter_Ekf_GetState(
  const filter_ekf_t *filter_h,
  filter_ekf_state_t *state
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_EKF_H
