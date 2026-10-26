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
 * @param filter_h 对象。
 * @param name 名称。
 * @param config 默认配置，不能为空。
 * @retval true 参数有效并完成初始化。
 * @retval false 对象或配置无效。
 */
bool Filter_Ekf_Init(
  filter_ekf_t *filter_h,
  const char *name,
  const filter_ekf_config_t *config
);

/**
 * @brief 提交一个在线参数。
 * @param filter_h 对象。
 * @param parameter 参数身份。
 * @param value 新值，单位和量纲由参数身份决定。
 * @retval true 参数有效并将在下一帧开始时生效。
 * @retval false 参数身份、数值、单参数范围或组合范围无效。
 * @note 该函数可从参数任务调用，不会修改正在执行的滤波状态。
 * @note 多个未提交参数按调用顺序合并校验；任一调用失败时本次值不生效。
 */
bool Filter_Ekf_SetParameter(
  filter_ekf_t *filter_h,
  filter_ekf_parameter_t parameter,
  float value
);

/**
 * @brief 请求把所有参数恢复为初始化时的默认值。
 * @param filter_h 对象。
 * @note 参数在下一帧开始时原子生效。
 * @pre filter_h 非空。
 */
void Filter_Ekf_ResetParameters(filter_ekf_t *filter_h);

/**
 * @brief 复制当前 EKF 参数。
 * @param filter_h 对象。
 * @param config 输出参数。
 * @pre filter_h 和 config 非空。
 */
void Filter_Ekf_GetConfig(
  const filter_ekf_t *filter_h,
  filter_ekf_config_t *config
);

/**
 * @brief 复制最近一次 EKF 状态。
 * @param filter_h 对象。
 * @param state 输出状态。
 * @pre filter_h 和 state 非空。
 */
void Filter_Ekf_GetState(
  const filter_ekf_t *filter_h,
  filter_ekf_state_t *state
);

/** @} */

#endif //LAYERED_EMBEDDED_C_FILTER_EKF_H
