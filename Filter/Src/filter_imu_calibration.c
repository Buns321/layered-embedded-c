//
// 由 Codex 创建于 2026/10/27。
//

/**
 * @file filter_imu_calibration.c
 * @brief 统一 IMU 六轴校正滤波器实现。
 * @ingroup filter
 */

#include "filter_imu_calibration.h"
#include "system_critical.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define FILTER_IMU_CAL_GYRO_X_CHANNEL   3u
#define FILTER_IMU_CAL_GYRO_Y_CHANNEL   4u
#define FILTER_IMU_CAL_GYRO_Z_CHANNEL   5u
#define FILTER_IMU_CAL_ACCEL_X_CHANNEL  6u
#define FILTER_IMU_CAL_ACCEL_Y_CHANNEL  7u
#define FILTER_IMU_CAL_ACCEL_Z_CHANNEL  8u
#define FILTER_IMU_CAL_SCALE_MIN        0.1f
#define FILTER_IMU_CAL_SCALE_MAX        10.0f
#define FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS 720.0f

/**
 * @brief 校验 IMU 校正配置。
 * @param config 只读候选配置，不能为 NULL；调用方保留所有权。
 * @retval true 三轴偏移和比例有限、比例在 0.1..10、陀螺零偏在 ±720 deg/s 内。
 * @retval false 任一轴含非有限值、比例越界或零偏越界。
 * @pre config 在调用期间可读且覆盖完整三轴。
 * @post 不修改配置，也不产生任何全局状态变化。
 * @note 常量上限见本文件 FILTER_IMU_CAL_* 宏；不分配动态内存。
 */
static bool filter_imucalibration_configisvalid(
  const filter_imu_calibration_config_t *config
) {
  assert(config);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    if (
      !SYS_ISFINITE(config->accel_offset_mps2[axis]) ||
      !SYS_ISFINITE(config->accel_scale[axis]) ||
      !SYS_ISFINITE(config->gyro_bias_dps[axis]) ||
      config->accel_scale[axis] < FILTER_IMU_CAL_SCALE_MIN ||
      config->accel_scale[axis] > FILTER_IMU_CAL_SCALE_MAX ||
      SYS_ABS(config->gyro_bias_dps[axis]) > FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS
    ) {
      return false;
    }
  }

  return true;
}

/**
 * @brief 对单轴加速度执行 (value-offset)*scale。
 * @param value 原始加速度，单位 m/s²；不要求有限，非有限结果由调用方丢弃通道。
 * @param offset 加速度零偏，单位 m/s²；应由配置校验保证有限。
 * @param scale 无量纲比例系数；应由配置校验保证在 0.1..10。
 * @return 校正后加速度，单位 m/s²；输入或系数非有限时结果可能非有限。
 * @pre offset 和 scale 已通过配置校验；调用方负责最终有限性检查。
 * @post 不修改任何对象或配置，只返回本次计算结果。
 * @note 纯计算函数；不分配动态内存。
 */
static float filter_imucalibration_apply(
  float value,
  float offset,
  float scale
) {
  return (value - offset) * scale;
}

/**
 * @brief 重置 IMU 校正内部状态。
 * @param filter_h 已初始化的校正滤波器基对象，不能为 NULL。
 * @pre 当前没有并发 update；校正参数保持有效。
 * @post corrected_frames 清零，累计配置保持不变。
 * @note 不分配动态内存；不修改 filter_base 或六轴参数。
 */
static void filter_imucalibration_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_imu_calibration_t *imu_calibration_h =
    (filter_imu_calibration_t *)filter_h;
  imu_calibration_h->corrected_frames = 0u;
}

/**
 * @brief 对 IMU 固定通道 3..8 应用陀螺零偏与加速度校正。
 * @param filter_h 已初始化的校正滤波器基对象，不能为 NULL。
 * @param input 只读输入样本；陀螺 XYZ 使用通道 3..5，加速度 XYZ 使用通道 6..8。
 * @param output 调用方拥有的输出样本，base 已预复制 input。
 * @pre 配置有效；update 在临界区内复制配置快照，input/output 在调用期间可访问。
 * @post 有效陀螺通道执行 raw-bias，有效加速度通道执行 (raw-offset)*scale；
 *       校正结果非有限的通道会清除有效位；corrected_frames 自增一次。
 * @note 陀螺输出仍为 deg/s，加速度输出为 m/s²；单写者调用，不分配动态内存。
 */
static void filter_imucalibration_update(
  filter_base_t *filter_h,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  assert(filter_h && input && output);

  filter_imu_calibration_t *imu_calibration_h =
    (filter_imu_calibration_t *)filter_h;
  filter_imu_calibration_config_t config;

  Sys_Critical_Enter();
  config = imu_calibration_h->config;
  Sys_Critical_Exit();

  const uint32_t gyro_channels[3] = {
    FILTER_IMU_CAL_GYRO_X_CHANNEL,
    FILTER_IMU_CAL_GYRO_Y_CHANNEL,
    FILTER_IMU_CAL_GYRO_Z_CHANNEL,
  };
  const uint32_t accel_channels[3] = {
    FILTER_IMU_CAL_ACCEL_X_CHANNEL,
    FILTER_IMU_CAL_ACCEL_Y_CHANNEL,
    FILTER_IMU_CAL_ACCEL_Z_CHANNEL,
  };

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    uint32_t gyro_mask = (uint32_t)1u << gyro_channels[axis];
    if ((output->valid_mask & gyro_mask) != 0u) {
      float corrected =
        output->value[gyro_channels[axis]] -
        config.gyro_bias_dps[axis];
      if (SYS_ISFINITE(corrected)) {
        output->value[gyro_channels[axis]] = corrected;
      } else {
        output->valid_mask &= ~gyro_mask;
      }
    }

    uint32_t accel_mask = (uint32_t)1u << accel_channels[axis];
    if ((output->valid_mask & accel_mask) != 0u) {
      float corrected = filter_imucalibration_apply(
        output->value[accel_channels[axis]],
        config.accel_offset_mps2[axis],
        config.accel_scale[axis]
      );
      if (SYS_ISFINITE(corrected)) {
        output->value[accel_channels[axis]] = corrected;
      } else {
        output->valid_mask &= ~accel_mask;
      }
    }
  }

  imu_calibration_h->corrected_frames++;
}

static const filter_ops_t filter_ops_imu_calibration = {
  .update = filter_imucalibration_update,
  .reset = filter_imucalibration_reset,
};

/**
 * @brief 初始化六轴校正滤波器。
 * @param filter_h 待初始化的校正滤波器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖滤波器对象。
 * @param config 非空初始参数；加速度比例系数每轴须在 0.1..10，陀螺零偏绝对值不超过 720 deg/s。
 * @retval true 参数有效，对象已清零、绑定并完成 reset。
 * @retval false filter_h、name 或 config 为空，或配置含非有限值/超范围比例/超范围零偏；对象保持不变。
 * @pre 只在任务初始化阶段调用，且 filter_h 未被其他上下文使用。
 * @post 成功时 corrected_frames 为 0，配置被复制并持久保存；失败时不修改 filter_h。
 * @note 不分配动态内存；输入通道 schema 与 IMU 固定通道 3..8 一致。
 */
bool Filter_ImuCalibration_Init(
  filter_imu_calibration_t *filter_h,
  const char *name,
  const filter_imu_calibration_config_t *config
) {
  if (
    filter_h == NULL ||
    name == NULL ||
    config == NULL ||
    !filter_imucalibration_configisvalid(config)
  ) {
    return false;
  }

  memset(filter_h, 0, sizeof(*filter_h));
  Filter_Base_Init(
    &filter_h->filter_base,
    name,
    &filter_ops_imu_calibration
  );
  filter_h->config = *config;
  Filter_Base_Reset(&filter_h->filter_base);
  return true;
}

/**
 * @brief 在帧边界更新六轴校正参数。
 * @param filter_h 已初始化的校正滤波器，不能为 NULL。
 * @param config 非空候选参数；校验规则与 Filter_ImuCalibration_Init 相同。
 * @retval true 参数有效，已在临界区内完整提交。
 * @retval false 任一指针为空或参数无效，原配置保持不变。
 * @pre filter_h 已成功初始化；调用方拥有 config，函数只读取它。
 * @post 成功时后续 update 使用新配置；update 通过临界区复制配置，不会观察到半更新状态。
 * @note 可从参数任务调用；配置提交与帧内读取由 Sys_Critical 串行化，不分配动态内存。
 */
bool Filter_ImuCalibration_SetConfig(
  filter_imu_calibration_t *filter_h,
  const filter_imu_calibration_config_t *config
) {
  if (
    filter_h == NULL ||
    config == NULL ||
    !filter_imucalibration_configisvalid(config)
  ) {
    return false;
  }

  Sys_Critical_Enter();
  filter_h->config = *config;
  Sys_Critical_Exit();
  return true;
}

/**
 * @brief 复制当前六轴校正参数。
 * @param filter_h 已初始化的校正滤波器，不能为 NULL。
 * @param config 调用方拥有的输出存储，不能为 NULL。
 * @pre filter_h 已成功初始化；config 可写且不与滤波器内部配置重叠。
 * @post config 获得临界区内的完整配置快照，滤波器状态不变。
 * @note 可从其他任务读取；不分配动态内存。
 */
void Filter_ImuCalibration_GetConfig(
  const filter_imu_calibration_t *filter_h,
  filter_imu_calibration_config_t *config
) {
  assert(filter_h && config);

  Sys_Critical_Enter();
  *config = filter_h->config;
  Sys_Critical_Exit();
}
