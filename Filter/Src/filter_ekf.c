//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file filter_ekf.c
 * @brief 七状态四元数 EKF 的接口壳层与占位核心。
 * @ingroup filter
 */

#include "filter_ekf.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define FILTER_EKF_DEG_TO_RAD 0.01745329251994329577f
#define FILTER_EKF_RAD_TO_DEG 57.295779513082320876f
#define FILTER_EKF_IMU_YAW_CHANNEL   0u
#define FILTER_EKF_IMU_ROLL_CHANNEL  1u
#define FILTER_EKF_IMU_PITCH_CHANNEL 2u
#define FILTER_EKF_IMU_GYRO_X_CHANNEL 3u
#define FILTER_EKF_IMU_GYRO_Y_CHANNEL 4u
#define FILTER_EKF_IMU_GYRO_Z_CHANNEL 5u
#define FILTER_EKF_IMU_ACCEL_X_CHANNEL 6u
#define FILTER_EKF_IMU_ACCEL_Y_CHANNEL 7u
#define FILTER_EKF_IMU_ACCEL_Z_CHANNEL 8u
#define FILTER_EKF_ALL_PARAMETER_MASK \
  ((uint32_t)((1u << FILTER_EKF_PARAM_COUNT) - 1u))

/**
 * @brief 判断数值为正有限值。
 * @param value 待检查的浮点值，不限定物理量。
 * @retval true value 既非 NaN/Inf 且严格大于 0。
 * @retval false value 为 NaN、Inf、0 或负数。
 * @pre 无；函数只读取标量参数。
 * @post 不修改 EKF 状态、配置、输入样本或全局状态。
 * @note 用于 EKF 噪声方差和正门限校验；纯函数，不分配动态内存。
 */
static bool filter_ekf_valueispositivefinite(float value) {
  return SYS_ISFINITE(value) && value > 0.0f;
}

/**
 * @brief 校验 EKF 配置范围和有限性。
 * @param config 只读候选配置，可为 NULL；调用方保留所有权。
 * @retval true 所有噪声/限幅为正有限值，accel_norm_min_mps2>0、max>min，max_dt_s>=min_dt_s 且均有限。
 * @retval false config 为空或任一量不满足上述约束。
 * @pre config 在非空时可读且覆盖完整配置结构。
 * @post 不修改配置或 EKF 状态。
 * @note 纯校验函数；不分配动态内存。
 */
static bool filter_ekf_configisvalid(const filter_ekf_config_t *config) {
  if (config == NULL) {
    return false;
  }

  return
    filter_ekf_valueispositivefinite(config->angle_process_noise_rad2) &&
    filter_ekf_valueispositivefinite(config->bias_process_noise_radps2) &&
    filter_ekf_valueispositivefinite(
      config->accel_measurement_noise_mps2_2
    ) &&
    filter_ekf_valueispositivefinite(
      config->initial_angle_variance_rad2
    ) &&
    filter_ekf_valueispositivefinite(
      config->initial_bias_variance_radps2
    ) &&
    SYS_ISFINITE(config->accel_norm_min_mps2) &&
    SYS_ISFINITE(config->accel_norm_max_mps2) &&
    config->accel_norm_min_mps2 > 0.0f &&
    config->accel_norm_max_mps2 > config->accel_norm_min_mps2 &&
    filter_ekf_valueispositivefinite(config->gyro_limit_radps) &&
    filter_ekf_valueispositivefinite(config->bias_limit_radps) &&
    filter_ekf_valueispositivefinite(config->min_dt_s) &&
    filter_ekf_valueispositivefinite(config->max_dt_s) &&
    config->max_dt_s >= config->min_dt_s;
}

/**
 * @brief 定位配置结构中可设置的字段。
 * @param config 可写的候选配置，不能为 NULL。
 * @param parameter 参数身份；FILTER_EKF_PARAM_COUNT 及更大值无对应字段。
 * @return 指向对应 float 字段的指针；parameter 非法时返回 NULL。
 * @pre config 在调用期间保持有效，且不与活动配置共享可变存储。
 * @post 返回的指针指向 config 内部字段，生命周期与 config 相同；函数不修改字段值。
 * @note 仅用于 SetParameter 在候选副本上合并参数；不分配动态内存。
 */
static float *filter_ekf_configfield(
  filter_ekf_config_t *config,
  filter_ekf_parameter_t parameter
) {
  assert(config);

  switch (parameter) {
    case FILTER_EKF_PARAM_ANGLE_PROCESS_NOISE:
      return &config->angle_process_noise_rad2;
    case FILTER_EKF_PARAM_BIAS_PROCESS_NOISE:
      return &config->bias_process_noise_radps2;
    case FILTER_EKF_PARAM_ACCEL_MEASUREMENT_NOISE:
      return &config->accel_measurement_noise_mps2_2;
    case FILTER_EKF_PARAM_INITIAL_ANGLE_VARIANCE:
      return &config->initial_angle_variance_rad2;
    case FILTER_EKF_PARAM_INITIAL_BIAS_VARIANCE:
      return &config->initial_bias_variance_radps2;
    case FILTER_EKF_PARAM_ACCEL_NORM_MIN:
      return &config->accel_norm_min_mps2;
    case FILTER_EKF_PARAM_ACCEL_NORM_MAX:
      return &config->accel_norm_max_mps2;
    case FILTER_EKF_PARAM_GYRO_LIMIT:
      return &config->gyro_limit_radps;
    case FILTER_EKF_PARAM_BIAS_LIMIT:
      return &config->bias_limit_radps;
    case FILTER_EKF_PARAM_MIN_DT:
      return &config->min_dt_s;
    case FILTER_EKF_PARAM_MAX_DT:
      return &config->max_dt_s;
    default:
      return NULL;
  }
}

/**
 * @brief 初始化 EKF 协方差矩阵。
 * @param filter_h 已初始化配置的 EKF 对象，不能为 NULL。
 * @pre config 中的初始姿态/零偏方差已通过校验。
 * @post covariance 全零，四元数 4 个对角项设为 initial_angle_variance_rad2，
 *       三个陀螺零偏对角项设为 initial_bias_variance_radps2。
 * @note 不分配动态内存；不修改状态向量或时间状态。
 */
static void filter_ekf_initializecovariance(filter_ekf_t *filter_h) {
  assert(filter_h);

  memset(
    filter_h->covariance,
    0,
    sizeof(filter_h->covariance)
  );

  for (uint8_t index = 0u; index < 4u; index++) {
    filter_h->covariance[index][index] =
      filter_h->config.initial_angle_variance_rad2;
  }
  for (uint8_t index = 4u; index < FILTER_EKF_STATE_DIMENSION; index++) {
    filter_h->covariance[index][index] =
      filter_h->config.initial_bias_variance_radps2;
  }
}

/**
 * @brief 在安全点应用待处理的 EKF 配置。
 * @param filter_h 已初始化的 EKF 对象，不能为 NULL。
 * @pre 仅在 EKF 更新的安全点调用；pending_config 与 pending_mask 可在临界区内一致读取。
 * @post pending_mask 清零；候选配置有效时替换 active config，否则 pending_config 回退到 active config。
 * @note 不修改协方差或姿态；不分配动态内存，调用方负责与其他参数写入串行化。
 */
static void filter_ekf_applypendingconfig(filter_ekf_t *filter_h) {
  assert(filter_h);

  filter_ekf_config_t pending_config;
  uint32_t pending_mask;

  Sys_Critical_Enter();
  pending_config = filter_h->pending_config;
  pending_mask = filter_h->pending_mask;
  filter_h->pending_mask = 0u;
  Sys_Critical_Exit();

  if (pending_mask == 0u) {
    return;
  }

  if (filter_ekf_configisvalid(&pending_config)) {
    filter_h->config = pending_config;
  } else {
    filter_h->pending_config = filter_h->config;
  }
}

/**
 * @brief 把 EKF 时间步限制到允许范围。
 * @param filter_h 已初始化的 EKF 对象，不能为 NULL。
 * @param now_ms 当前单调毫秒时间戳；使用无符号减法计算与上次的间隔。
 * @return 本次融合时间步，单位 s；首帧返回 min_dt_s，后续夹紧到 [min_dt_s, max_dt_s]。
 * @pre config 的 dt 范围有效；调用方保证 now_ms 来自单调时基。
 * @post last_time_ms 更新为 now_ms，has_time 置为 true。
 * @note 不分配动态内存；本函数会推进时间状态，只应由单写者 update 调用。
 */
static float filter_ekf_clampdeltatime(
  filter_ekf_t *filter_h,
  uint32_t now_ms
) {
  assert(filter_h);

  float dt_s = filter_h->config.min_dt_s;

  if (filter_h->has_time) {
    uint32_t elapsed_ms = now_ms - filter_h->last_time_ms;
    dt_s = (float)elapsed_ms * 0.001f;
    dt_s = SYS_CLAMP(
      dt_s,
      filter_h->config.min_dt_s,
      filter_h->config.max_dt_s
    );
  }

  filter_h->last_time_ms = now_ms;
  filter_h->has_time = true;
  return dt_s;
}

/**
 * @brief 检查输入帧是否包含全部必需有效通道。
 * @param input 只读输入样本，不能为 NULL。
 * @retval true 陀螺 XYZ（通道 3..5）和加速度 XYZ（通道 6..8）的 valid_mask 全部置位。
 * @retval false 任一必需通道缺失有效位。
 * @pre input 的 valid_mask 已由 Filter_Base_Update 校验为合法掩码。
 * @post 不修改样本或 EKF 状态；本函数不检查通道数值是否有限。
 * @note 纯查询函数；不分配动态内存。
 */
static bool filter_ekf_inputframeisusable(const filter_sample_t *input) {
  assert(input);
  const uint32_t required_mask =
    (uint32_t)(1u << FILTER_EKF_IMU_GYRO_X_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_GYRO_Y_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_GYRO_Z_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_ACCEL_X_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_ACCEL_Y_CHANNEL) |
    (uint32_t)(1u << FILTER_EKF_IMU_ACCEL_Z_CHANNEL);
  return (input->valid_mask & required_mask) == required_mask;
}

/**
 * @brief 执行 EKF 预测与校正核心步骤。
 * @param filter_h 已初始化的 EKF 对象，不能为 NULL。
 * @param gyro_radps 三轴陀螺角速度，单位 rad/s；当前占位核心不读取。
 * @param accel_mps2 三轴加速度，单位 m/s²；当前占位核心不读取。
 * @param dt_s 本次融合时间步，单位 s；已由调用方夹紧到配置范围。
 * @pre 输入通道完整且配置已应用；当前函数是占位实现。
 * @post 发布单位四元数、零姿态，state.placeholder 和 state.valid 均为 true；不推进协方差。
 * @warning 本核心未实现预测/校正，姿态不可用于闭环控制。
 * @note 不分配动态内存；后续替换核心时必须维持状态单位契约。
 */
static void filter_ekf_coreupdate(
  filter_ekf_t *filter_h,
  const float gyro_radps[3],
  const float accel_mps2[3],
  float dt_s
) {
  assert(filter_h && gyro_radps && accel_mps2);
  (void)gyro_radps;
  (void)accel_mps2;
  (void)dt_s;

  // EKF 预测与校正由使用者在此实现；此阶段只发布确定的单位四元数占位值。
  filter_h->state.quaternion[0] = 1.0f;
  filter_h->state.quaternion[1] = 0.0f;
  filter_h->state.quaternion[2] = 0.0f;
  filter_h->state.quaternion[3] = 0.0f;
  filter_h->state.roll_rad = 0.0f;
  filter_h->state.pitch_rad = 0.0f;
  filter_h->state.yaw_rad = 0.0f;
  filter_h->state.placeholder = true;
  filter_h->state.valid = true;
}

/**
 * @brief 发布 EKF 姿态结果到 IMU 姿态通道。
 * @param filter_h 已初始化的 EKF 对象，不能为 NULL。
 * @param output 调用方拥有的输出样本，base 已预复制 input；不能为 NULL。
 * @pre state.roll_rad、pitch_rad、yaw_rad 已由核心更新。
 * @post 通道 0..2 分别写入 yaw/roll/pitch（deg）；state.valid 为 true 时置有效位，否则清除。
 * @note 内部角度为 rad，输出边界转为 deg；不修改角速度通道或时间戳，不分配动态内存。
 */
static void filter_ekf_publishattitude(
  filter_ekf_t *filter_h,
  filter_sample_t *output
) {
  assert(filter_h && output);

  const uint32_t yaw_mask =
    (uint32_t)1u << FILTER_EKF_IMU_YAW_CHANNEL;
  const uint32_t roll_mask =
    (uint32_t)1u << FILTER_EKF_IMU_ROLL_CHANNEL;
  const uint32_t pitch_mask =
    (uint32_t)1u << FILTER_EKF_IMU_PITCH_CHANNEL;

  output->value[FILTER_EKF_IMU_YAW_CHANNEL] =
    filter_h->state.yaw_rad * FILTER_EKF_RAD_TO_DEG;
  output->value[FILTER_EKF_IMU_ROLL_CHANNEL] =
    filter_h->state.roll_rad * FILTER_EKF_RAD_TO_DEG;
  output->value[FILTER_EKF_IMU_PITCH_CHANNEL] =
    filter_h->state.pitch_rad * FILTER_EKF_RAD_TO_DEG;

  if (filter_h->state.valid) {
    output->valid_mask |= yaw_mask | roll_mask | pitch_mask;
  } else {
    output->valid_mask &= ~(yaw_mask | roll_mask | pitch_mask);
  }
}

/**
 * @brief 重置 EKF 状态与协方差。
 * @param filter_h 已初始化的 EKF 基对象，不能为 NULL。
 * @pre 当前没有并发 update；active config 保持有效。
 * @post state 清零为单位四元数且 placeholder=true，last_time_ms=0、has_time=false，协方差重新初始化。
 * @note 不修改 config、default_config 或 pending_config；不分配动态内存。
 */
static void filter_ekf_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_ekf_t *filter_ekf_h = (filter_ekf_t *)filter_h;

  filter_ekf_h->state = (filter_ekf_state_t){
    .quaternion = {1.0f, 0.0f, 0.0f, 0.0f},
    .placeholder = true,
  };
  filter_ekf_h->last_time_ms = 0u;
  filter_ekf_h->has_time = false;
  filter_ekf_initializecovariance(filter_ekf_h);
}

/**
 * @brief 执行 EKF 单步更新。
 * @param filter_h 已初始化的 EKF 基对象，不能为 NULL。
 * @param input 只读 IMU 固定通道样本，不能为 NULL；需要通道 3..8 全部有效。
 * @param output 调用方拥有的输出样本，base 已预复制 input。
 * @pre 单写者调用；time_ms 单调；配置中的 dt 范围有效。
 * @post 先应用待提交配置并推进 dt；缺通道时 state.valid=false 且姿态位清除；
 *       通道完整时执行核心、增加 update_count 并发布姿态。
 * @warning 当前核心为占位实现，即使发布占位姿态，结果也不可用于闭环控制。
 * @note 不分配动态内存；配置快照交换使用短临界区。
 */
static void filter_ekf_update(
  filter_base_t *filter_h,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  assert(filter_h && input && output);
  filter_ekf_t *filter_ekf_h = (filter_ekf_t *)filter_h;

  filter_ekf_applypendingconfig(filter_ekf_h);
  float dt_s = filter_ekf_clampdeltatime(filter_ekf_h, input->time_ms);

  if (!filter_ekf_inputframeisusable(input)) {
    filter_ekf_h->state.valid = false;
    filter_ekf_h->state.dt_s = dt_s;
    filter_ekf_publishattitude(filter_ekf_h, output);
    return;
  }

  float gyro_radps[3] = {
    input->value[FILTER_EKF_IMU_GYRO_X_CHANNEL] *
      FILTER_EKF_DEG_TO_RAD,
    input->value[FILTER_EKF_IMU_GYRO_Y_CHANNEL] *
      FILTER_EKF_DEG_TO_RAD,
    input->value[FILTER_EKF_IMU_GYRO_Z_CHANNEL] *
      FILTER_EKF_DEG_TO_RAD,
  };
  float accel_mps2[3] = {
    input->value[FILTER_EKF_IMU_ACCEL_X_CHANNEL],
    input->value[FILTER_EKF_IMU_ACCEL_Y_CHANNEL],
    input->value[FILTER_EKF_IMU_ACCEL_Z_CHANNEL],
  };

  filter_ekf_coreupdate(
    filter_ekf_h,
    gyro_radps,
    accel_mps2,
    dt_s
  );
  filter_ekf_h->state.dt_s = dt_s;
  filter_ekf_h->state.update_count++;
  filter_ekf_publishattitude(filter_ekf_h, output);
}

static const filter_ops_t filter_ops_ekf = {
  .update = filter_ekf_update,
  .reset = filter_ekf_reset,
};

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
) {
  if (
    filter_h == NULL ||
    name == NULL ||
    !filter_ekf_configisvalid(config)
  ) {
    return false;
  }

  memset(filter_h, 0, sizeof(*filter_h));
  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_ekf);
  filter_h->config = *config;
  filter_h->default_config = *config;
  filter_h->pending_config = *config;
  filter_h->pending_mask = 0u;
  Filter_Base_Reset(&filter_h->filter_base);
  return true;
}

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
) {
  if (
    filter_h == NULL ||
    (uint32_t)parameter >= (uint32_t)FILTER_EKF_PARAM_COUNT ||
    !SYS_ISFINITE(value)
  ) {
    return false;
  }

  filter_ekf_config_t candidate;
  uint32_t pending_mask;

  Sys_Critical_Enter();
  candidate = filter_h->pending_mask != 0u
    ? filter_h->pending_config
    : filter_h->config;
  pending_mask = filter_h->pending_mask;
  Sys_Critical_Exit();

  float *field = filter_ekf_configfield(&candidate, parameter);
  if (field == NULL) {
    return false;
  }
  *field = value;

  if (!filter_ekf_configisvalid(&candidate)) {
    return false;
  }

  Sys_Critical_Enter();
  filter_h->pending_config = candidate;
  filter_h->pending_mask = pending_mask | ((uint32_t)1u << parameter);
  Sys_Critical_Exit();
  return true;
}

/**
 * @brief 请求把所有参数恢复为初始化时的默认值。
 * @param filter_h 已初始化的 EKF，不能为 NULL。
 * @pre 单参数写者调用；default_config 已由 Filter_Ekf_Init 保存。
 * @post pending_config 被替换为 default_config，pending_mask 标记全部参数。
 * @note 新参数在下一帧开始时原子应用；不立即改变 active config 或协方差，不分配动态内存。
 */
void Filter_Ekf_ResetParameters(filter_ekf_t *filter_h) {
  assert(filter_h);

  Sys_Critical_Enter();
  filter_h->pending_config = filter_h->default_config;
  filter_h->pending_mask = FILTER_EKF_ALL_PARAMETER_MASK;
  Sys_Critical_Exit();
}

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
) {
  assert(filter_h && config);

  Sys_Critical_Enter();
  *config = filter_h->config;
  Sys_Critical_Exit();
}

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
) {
  assert(filter_h && state);

  Sys_Critical_Enter();
  *state = filter_h->state;
  Sys_Critical_Exit();
}
