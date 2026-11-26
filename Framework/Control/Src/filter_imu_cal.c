/**
 * @file filter_imu_cal.c
 * @brief IMU 六轴零偏与比例校正滤波器实现。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          对通道 3..5（陀螺仪 X/Y/Z，单位 deg/s）扣除零偏，对通道 6..8（加速度计 X/Y/Z，
 *          单位 m/s^2）执行零偏扣除与比例缩放，并使用 SYS_SNAPSHOT_READ/WRITE 保障
 *          运行期标定参数热更新的原子性。
 * @ingroup Framework_Control
 */

#include "filter_imu_cal.h"
#include "sys_math.h"
#include "sys_snapshot.h"
#include <stdbool.h>
#include <stddef.h>

/** @brief IMU 样本中陀螺仪 X 轴通道索引（单位：deg/s）。 */
#define FILTER_IMU_CAL_GYRO_X_CH 3U
/** @brief IMU 样本中陀螺仪 Y 轴通道索引（单位：deg/s）。 */
#define FILTER_IMU_CAL_GYRO_Y_CH 4U
/** @brief IMU 样本中陀螺仪 Z 轴通道索引（单位：deg/s）。 */
#define FILTER_IMU_CAL_GYRO_Z_CH 5U
/** @brief IMU 样本中加速度计 X 轴通道索引（单位：m/s^2）。 */
#define FILTER_IMU_CAL_ACCEL_X_CH 6U
/** @brief IMU 样本中加速度计 Y 轴通道索引（单位：m/s^2）。 */
#define FILTER_IMU_CAL_ACCEL_Y_CH 7U
/** @brief IMU 样本中加速度计 Z 轴通道索引（单位：m/s^2）。 */
#define FILTER_IMU_CAL_ACCEL_Z_CH 8U
/** @brief 加速度比例因子允许下限（无量纲）。 */
#define FILTER_IMU_CAL_SCALE_MIN 0.1f
/** @brief 加速度比例因子允许上限（无量纲）。 */
#define FILTER_IMU_CAL_SCALE_MAX 10.0f
/** @brief 陀螺仪零偏绝对值允许上限（单位：deg/s）。 */
#define FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS 720.0f

/**
 * @brief 校验六轴标定参数是否全部有限且在合理物理量程内。
 * @param[in] cfg 待校验的标定参数指针（非 NULL）。
 * @return 参数是否合法。
 * @retval true  全部轴向的零偏、比例因子均合法。
 * @retval false 存在非有限数或比例因子/陀螺零偏越界。
 */
static bool filter_imucal_cfgisvalid(const filter_imu_cal_cfg_t *cfg) {
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    if (!SYS_ISFINITE(cfg->accel_offset_mps2[axis]) || !SYS_ISFINITE(cfg->accel_scale[axis]) ||
        !SYS_ISFINITE(cfg->gyro_bias_dps[axis]) ||
        cfg->accel_scale[axis] < FILTER_IMU_CAL_SCALE_MIN ||
        cfg->accel_scale[axis] > FILTER_IMU_CAL_SCALE_MAX ||
        SYS_ABS(cfg->gyro_bias_dps[axis]) > FILTER_IMU_CAL_GYRO_BIAS_MAX_DPS) {
      return false;
    }
  }
  return true;
}

/**
 * @brief IMU 校正滤波器虚表 init 回调：校验并拷贝初始标定参数。
 * @param[in,out] filter_h      IMU 校正滤波器基类指针（实际类型为 filter_imu_cal_t*）。
 * @param[in]     filter_config 构造器配置（data 指向 filter_imu_cal_cfg_t）。
 * @return 初始化状态码。
 */
static filter_status_t filter_imucal_init(filter_base_t *filter_h,
                                          const filter_config_t *filter_config) {
  filter_imu_cal_t *cal = (filter_imu_cal_t *)filter_h;
  const filter_imu_cal_cfg_t *cfg = (const filter_imu_cal_cfg_t *)filter_config->data;

  if (cfg == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (!filter_imucal_cfgisvalid(cfg)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  cal->cfg = *cfg;
  return FILTER_STATUS_OK;
}

/**
 * @brief IMU 校正滤波器虚表 reset 回调：清零已校正帧计数（保留标定参数）。
 * @param[in,out] filter_h IMU 校正滤波器基类指针。
 */
static void filter_imucal_reset(filter_base_t *filter_h) {
  filter_imu_cal_t *cal = (filter_imu_cal_t *)filter_h;
  cal->corrected_frames = 0U;
}

/**
 * @brief IMU 校正滤波器虚表 update 回调：读取参数快照并就地校正有效六轴通道。
 * @param[in,out] filter_h IMU 校正滤波器基类指针。
 * @param[in,out] sample   样本指针（仅对 valid_mask 置位的 3..8 通道执行校正）。
 */
static void filter_imucal_update(filter_base_t *filter_h, filter_sample_t *sample) {
  static const uint8_t s_gyro_ch[3] = {
      FILTER_IMU_CAL_GYRO_X_CH,
      FILTER_IMU_CAL_GYRO_Y_CH,
      FILTER_IMU_CAL_GYRO_Z_CH,
  };
  static const uint8_t s_accel_ch[3] = {
      FILTER_IMU_CAL_ACCEL_X_CH,
      FILTER_IMU_CAL_ACCEL_Y_CH,
      FILTER_IMU_CAL_ACCEL_Z_CH,
  };

  filter_imu_cal_t *cal = (filter_imu_cal_t *)filter_h;
  filter_imu_cal_cfg_t cfg;
  // 通过快照临界区拷贝整套标定参数，防止并发调用 SetConfig 时读到撕裂的半更新配置
  SYS_SNAPSHOT_READ(&cfg, &cal->cfg);

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    // 陀螺仪校正：仅处理有效通道，扣除静态零偏后经 WriteFiniteChannel 写回（自动防溢出）
    uint8_t g_ch = s_gyro_ch[axis];
    if ((sample->valid_mask & Filter_Sample_GetChannelMask(g_ch)) != 0U) {
      float corrected = sample->value[g_ch] - cfg.gyro_bias_dps[axis];
      (void)Filter_Sample_WriteFiniteChannel(sample, g_ch, corrected);
    }

    // 加速度计校正：先去零偏再乘比例因子，保持未置位通道（如姿态角 0..2）原样透传
    uint8_t a_ch = s_accel_ch[axis];
    if ((sample->valid_mask & Filter_Sample_GetChannelMask(a_ch)) != 0U) {
      float corrected = (sample->value[a_ch] - cfg.accel_offset_mps2[axis]) * cfg.accel_scale[axis];
      (void)Filter_Sample_WriteFiniteChannel(sample, a_ch, corrected);
    }
  }

  cal->corrected_frames++;
}

/** @brief IMU 六轴校正滤波器静态只读虚表。 */
static const filter_ops_t s_filter_ops_imu_cal = {
    .init = filter_imucal_init,
    .update = filter_imucal_update,
    .reset = filter_imucal_reset,
};

filter_status_t Filter_ImuCal_Init(filter_imu_cal_t *filter_h, const char *name,
                                   const filter_imu_cal_cfg_t *cfg) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  const filter_config_t config = {
      .name = name,
      .data = cfg,
  };
  return Filter_Base_Construct(&filter_h->base, &config, &s_filter_ops_imu_cal);
}

filter_status_t Filter_ImuCal_SetConfig(filter_imu_cal_t *filter_h,
                                        const filter_imu_cal_cfg_t *cfg) {
  if (filter_h == NULL || cfg == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (!filter_imucal_cfgisvalid(cfg)) {
    return FILTER_STATUS_INVALID_CONFIG;
  }
  SYS_SNAPSHOT_WRITE(&filter_h->cfg, cfg);
  return FILTER_STATUS_OK;
}

void Filter_ImuCal_GetConfig(const filter_imu_cal_t *filter_h, filter_imu_cal_cfg_t *cfg) {
  if (filter_h == NULL || cfg == NULL) {
    return;
  }
  SYS_SNAPSHOT_READ(cfg, &filter_h->cfg);
}
