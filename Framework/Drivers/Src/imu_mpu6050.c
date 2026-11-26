/**
 * @file imu_mpu6050.c
 * @brief MPU6050 I2C 6 轴 IMU 驱动实现。
 * @details 属于 Framework/Drivers 设备驱动层，实现寄存器配置与回读校验、
 *          14 字节突发读取（加速度/温度/陀螺仪）、静止方差校验零偏标定与
 *          imu_base_t 快照提交。
 * @ingroup Framework_Drivers
 */

#include "imu_mpu6050.h"

#if defined(HAL_I2C_MODULE_ENABLED) || defined(BSP_HOST_STUB)

#include "sys_critical.h"
#include "sys_time_window.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

/** @brief 采样率分频寄存器地址（SMPLRT_DIV）。 */
#define MPU6050_REG_SMPLRT_DIV 0x19U
/** @brief 数字低通滤波器配置寄存器地址（CONFIG）。 */
#define MPU6050_REG_CONFIG 0x1AU
/** @brief 陀螺仪量程配置寄存器地址（GYRO_CONFIG）。 */
#define MPU6050_REG_GYRO_CONFIG 0x1BU
/** @brief 加速度计量程配置寄存器地址（ACCEL_CONFIG）。 */
#define MPU6050_REG_ACCEL_CONFIG 0x1CU
/** @brief 中断使能寄存器地址（INT_ENABLE）。 */
#define MPU6050_REG_INT_ENABLE 0x38U
/** @brief 加速度 X 轴高字节起始地址（连续 14 字节突发读取起点）。 */
#define MPU6050_REG_ACCEL_XOUT_H 0x3BU
/** @brief 用户控制寄存器地址（USER_CTRL）。 */
#define MPU6050_REG_USER_CTRL 0x6AU
/** @brief 电源管理 1 寄存器地址（PWR_MGMT_1）。 */
#define MPU6050_REG_PWR_MGMT_1 0x6BU
/** @brief 电源管理 2 寄存器地址（PWR_MGMT_2）。 */
#define MPU6050_REG_PWR_MGMT_2 0x6CU
/** @brief 器件 ID 寄存器地址（WHO_AM_I）。 */
#define MPU6050_REG_WHO_AM_I 0x75U

/** @brief PWR_MGMT_1 软复位位掩码（DEVICE_RESET）。 */
#define MPU6050_PWR1_DEVICE_RESET 0x80U
/** @brief PWR_MGMT_1 时钟源选择：PLL with X-axis gyroscope reference。 */
#define MPU6050_PWR1_CLOCK_PLL_XGYRO 0x01U
/** @brief 最小轮询采样周期（单位：ms）。 */
#define MPU6050_SAMPLE_PERIOD_MIN_MS 1U
/** @brief 软复位后等待内部电路稳定的延时（单位：ms）。 */
#define MPU6050_RESET_DELAY_MS 100U
/** @brief 突发读取字节数：Accel(6B) + Temp(2B) + Gyro(6B) = 14 字节。 */
#define MPU6050_BURST_SIZE 14U
/** @brief 标准重力加速度常数（单位：m/s^2）。 */
#define MPU6050_GRAVITY_MPS2 9.80665f
/** @brief 静止校准允许的最大陀螺仪标准差阈值（单位：deg/s）。 */
#define MPU6050_CAL_MAX_STD_DPS 3.0f
/** @brief 静止校准加速度模长下限，过滤失重/异常读数（单位：m/s^2）。 */
#define MPU6050_CAL_MIN_ACCEL_MPS2 3.0f
/** @brief 静止校准加速度模长上限，过滤剧烈晃动（单位：m/s^2）。 */
#define MPU6050_CAL_MAX_ACCEL_MPS2 15.0f

/**
 * @brief 向 MPU6050 指定寄存器写入单字节并统计 I2C 错误。
 * @param[in,out] imu   MPU6050 对象指针。
 * @param[in]     reg   8 位寄存器地址。
 * @param[in]     value 待写入字节。
 * @return I2C 写操作状态码。
 */
static bsp_status_t mpu6050_write_reg(imu_mpu6050_t *imu, uint8_t reg, uint8_t value) {
  bsp_status_t st = BSP_I2C_MemWrite(&imu->cfg.i2c, reg, 1U, &value, 1U);
  if (st != BSP_OK) {
    imu->i2c_error_count++;
    imu->last_request_failed = true;
  }
  return st;
}

/**
 * @brief 从 MPU6050 指定寄存器连续读取多字节并统计 I2C 错误。
 * @param[in,out] imu  MPU6050 对象指针。
 * @param[in]     reg  起始寄存器地址。
 * @param[out]    data 接收缓冲区指针。
 * @param[in]     size 读取字节数。
 * @return I2C 读操作状态码。
 */
static bsp_status_t mpu6050_read_regs(imu_mpu6050_t *imu, uint8_t reg, uint8_t *data,
                                      uint16_t size) {
  bsp_status_t st = BSP_I2C_MemRead(&imu->cfg.i2c, reg, 1U, data, size);
  if (st != BSP_OK) {
    imu->i2c_error_count++;
    imu->last_request_failed = true;
  }
  return st;
}

/**
 * @brief 将大端序两字节原始数据解码为有符号 16 位整数。
 * @param[in] data 指向高字节在前、低字节在后的 2 字节数组。
 * @return 解码后的有符号 16 位整数。
 */
static int16_t mpu6050_be_i16(const uint8_t *data) {
  // 显式按无符号组合再做补码转换，避免 C11 下有符号左移溢出的未定义行为
  int32_t raw = ((int32_t)data[0] << 8) | (int32_t)data[1];
  if ((raw & 0x8000) != 0) {
    raw -= 0x10000;
  }
  return (int16_t)raw;
}

/**
 * @brief 根据加速度计量程枚举返回灵敏度（LSB/g）。
 * @param[in] range 加速度计量程枚举。
 * @return 灵敏度数值（单位：LSB/g；非法枚举返回 0.0f）。
 */
static float mpu6050_accel_lsb(mpu6050_accel_range_t range) {
  switch (range) {
  case MPU6050_ACCEL_RANGE_2G:
    return 16384.0f;
  case MPU6050_ACCEL_RANGE_4G:
    return 8192.0f;
  case MPU6050_ACCEL_RANGE_8G:
    return 4096.0f;
  case MPU6050_ACCEL_RANGE_16G:
    return 2048.0f;
  default:
    return 0.0f;
  }
}

/**
 * @brief 根据陀螺仪量程枚举返回灵敏度（LSB/(deg/s)）。
 * @param[in] range 陀螺仪量程枚举。
 * @return 灵敏度数值（单位：LSB/(deg/s)；非法枚举返回 0.0f）。
 */
static float mpu6050_gyro_lsb(mpu6050_gyro_range_t range) {
  switch (range) {
  case MPU6050_GYRO_RANGE_250DPS:
    return 131.0f;
  case MPU6050_GYRO_RANGE_500DPS:
    return 65.5f;
  case MPU6050_GYRO_RANGE_1000DPS:
    return 32.8f;
  case MPU6050_GYRO_RANGE_2000DPS:
    return 16.4f;
  default:
    return 0.0f;
  }
}

/**
 * @brief 根据 DLPF 配置返回陀螺仪内部基准输出速率（Hz）。
 * @param[in] dlpf 数字低通滤波器配置枚举。
 * @return 内部基准速率（DLPF 关闭即 256 Hz 时为 8000 Hz，其余为 1000 Hz）。
 */
static uint32_t mpu6050_gyro_rate_hz(mpu6050_dlpf_t dlpf) {
  return (dlpf == MPU6050_DLPF_256HZ) ? 8000U : 1000U;
}

/**
 * @brief 按配置的轴排列与符号将芯片坐标系三轴向量映射至车体坐标系。
 * @param[in]  cfg    驱动配置结构体指针。
 * @param[in]  input  芯片坐标系三轴分量。
 * @param[out] output 车体坐标系三轴分量。
 */
static void mpu6050_apply_axis_map(const imu_mpu6050_cfg_t *cfg, const float input[3],
                                   float output[3]) {
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    uint8_t src = cfg->axis_source[axis];
    output[axis] = input[src] * (float)cfg->axis_sign[axis];
  }
}

/**
 * @brief 突发读取 14 字节原始寄存器并换算为车体系工程单位样本。
 * @param[in,out] imu    MPU6050 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 * @param[out]    sample 转换后的工程单位样本输出指针。
 * @return I2C 读取状态码。
 */
static bsp_status_t mpu6050_acquire(imu_mpu6050_t *imu, uint32_t now_ms,
                                    mpu6050_raw_sample_t *sample) {
  uint8_t data[MPU6050_BURST_SIZE] = {0};
  // 从 ACCEL_XOUT_H (0x3B) 连续突发读取 14 字节，保证同一时刻的六轴与温度锁存一致性：
  // [0..5]=ACCEL_XYZ(大端), [6..7]=TEMP(大端), [8..13]=GYRO_XYZ(大端)
  bsp_status_t st = mpu6050_read_regs(imu, MPU6050_REG_ACCEL_XOUT_H, data, MPU6050_BURST_SIZE);
  if (st != BSP_OK) {
    return st;
  }

  float accel_g[3] = {
      (float)mpu6050_be_i16(&data[0]) / imu->accel_lsb_per_g,
      (float)mpu6050_be_i16(&data[2]) / imu->accel_lsb_per_g,
      (float)mpu6050_be_i16(&data[4]) / imu->accel_lsb_per_g,
  };
  float gyro_dps[3] = {
      (float)mpu6050_be_i16(&data[8]) / imu->gyro_lsb_per_dps,
      (float)mpu6050_be_i16(&data[10]) / imu->gyro_lsb_per_dps,
      (float)mpu6050_be_i16(&data[12]) / imu->gyro_lsb_per_dps,
  };

  float mapped_accel_g[3] = {0};
  float mapped_gyro_dps[3] = {0};
  mpu6050_apply_axis_map(&imu->cfg, accel_g, mapped_accel_g);
  mpu6050_apply_axis_map(&imu->cfg, gyro_dps, mapped_gyro_dps);

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    sample->accel_mps2[axis] = mapped_accel_g[axis] * MPU6050_GRAVITY_MPS2;
    sample->gyro_dps[axis] = mapped_gyro_dps[axis];
  }

  // 数据手册温度换算公式：T(°C) = TEMP_OUT / 340.0 + 36.53
  int16_t temp_raw = mpu6050_be_i16(&data[6]);
  sample->temperature_c = ((float)temp_raw / 340.0f) + 36.53f;
  sample->time_ms = now_ms;
  return BSP_OK;
}

/**
 * @brief 检查加速度模长是否处于静止重力加速度合理窗口 [3, 15] m/s^2 内。
 * @param[in] sample 工程单位样本指针。
 * @return 是否满足静止条件。
 */
static bool mpu6050_accel_is_stationary(const mpu6050_raw_sample_t *sample) {
  float norm_sq = 0.0f;
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    norm_sq += sample->accel_mps2[axis] * sample->accel_mps2[axis];
  }
  float norm = sqrtf(norm_sq);
  return norm >= MPU6050_CAL_MIN_ACCEL_MPS2 && norm <= MPU6050_CAL_MAX_ACCEL_MPS2;
}

/**
 * @brief 清空陀螺仪零偏校准累加器。
 * @param[in,out] imu MPU6050 对象指针。
 */
static void mpu6050_cal_reset(imu_mpu6050_t *imu) {
  imu->calibration_count = 0U;
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    imu->calibration_gyro_sum[axis] = 0.0f;
    imu->calibration_gyro_sum_sq[axis] = 0.0f;
  }
}

/**
 * @brief 结束零偏采集：校验各轴标准差并更新 gyro_bias_dps。
 * @param[in,out] imu MPU6050 对象指针。
 * @return 校准是否通过标准差门限。
 * @retval true  三轴标准差均 <= MPU6050_CAL_MAX_STD_DPS，均值已写入零偏。
 * @retval false 存在某轴抖动超标，拒绝更新零偏。
 */
static bool mpu6050_cal_finish(imu_mpu6050_t *imu) {
  float count = (float)imu->calibration_count;
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    float mean = imu->calibration_gyro_sum[axis] / count;
    float variance = (imu->calibration_gyro_sum_sq[axis] / count) - (mean * mean);
    if (variance < 0.0f) {
      variance = 0.0f;
    }
    if (sqrtf(variance) > MPU6050_CAL_MAX_STD_DPS) {
      return false;
    }
    imu->gyro_bias_dps[axis] = mean;
  }
  return true;
}

/**
 * @brief 写入时钟、分频、DLPF、量程寄存器并执行回读校验。
 * @param[in,out] imu MPU6050 对象指针。
 * @return 配置与回读校验状态码。
 */
static bsp_status_t mpu6050_configure(imu_mpu6050_t *imu) {
  const uint8_t gyro_cfg = (uint8_t)((uint8_t)imu->cfg.gyro_range << 3U);
  const uint8_t accel_cfg = (uint8_t)((uint8_t)imu->cfg.accel_range << 3U);

  bsp_status_t st = mpu6050_write_reg(imu, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_CLOCK_PLL_XGYRO);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_SMPLRT_DIV, imu->sample_rate_divider);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_CONFIG, (uint8_t)imu->cfg.dlpf);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_GYRO_CONFIG, gyro_cfg);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_ACCEL_CONFIG, accel_cfg);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_INT_ENABLE, 0U);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_USER_CTRL, 0U);
  if (st != BSP_OK) {
    return st;
  }
  st = mpu6050_write_reg(imu, MPU6050_REG_PWR_MGMT_2, 0U);
  if (st != BSP_OK) {
    return st;
  }

  // 逐一读回关键配置寄存器，防止 I2C 总线干扰或芯片未退出复位导致配置静默丢失
  uint8_t rb = 0U;
  st = mpu6050_read_regs(imu, MPU6050_REG_SMPLRT_DIV, &rb, 1U);
  if (st != BSP_OK || rb != imu->sample_rate_divider) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_CONFIG, &rb, 1U);
  if (st != BSP_OK || (rb & 0x07U) != (uint8_t)imu->cfg.dlpf) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_GYRO_CONFIG, &rb, 1U);
  if (st != BSP_OK || (rb & 0x18U) != gyro_cfg) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_ACCEL_CONFIG, &rb, 1U);
  if (st != BSP_OK || (rb & 0x18U) != accel_cfg) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }
  st = mpu6050_read_regs(imu, MPU6050_REG_PWR_MGMT_1, &rb, 1U);
  if (st != BSP_OK || (rb & 0x7FU) != MPU6050_PWR1_CLOCK_PLL_XGYRO) {
    return (st != BSP_OK) ? st : BSP_ERR_HAL;
  }

  return BSP_OK;
}

/**
 * @brief 将状态机转入错误态并置位失败标志。
 * @param[in,out] imu MPU6050 对象指针。
 */
static void mpu6050_enter_error(imu_mpu6050_t *imu) {
  imu->state = MPU6050_STATE_ERROR;
  imu->last_request_failed = true;
}

/**
 * @brief 推进静止陀螺仪零偏采集状态机。
 * @param[in,out] imu    MPU6050 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void mpu6050_process_calibration(imu_mpu6050_t *imu, uint32_t now_ms) {
  if (!Sys_Time_HasReachedMs(now_ms, imu->next_sample_ms)) {
    return;
  }
  imu->next_sample_ms = now_ms + imu->sample_period_ms;

  mpu6050_raw_sample_t sample = {0};
  if (mpu6050_acquire(imu, now_ms, &sample) != BSP_OK || !mpu6050_accel_is_stationary(&sample)) {
    imu->calibration_error_count++;
    mpu6050_enter_error(imu);
    return;
  }

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    imu->calibration_gyro_sum[axis] += sample.gyro_dps[axis];
    imu->calibration_gyro_sum_sq[axis] += sample.gyro_dps[axis] * sample.gyro_dps[axis];
  }
  imu->calibration_count++;

  if (imu->calibration_count < imu->cfg.calibration_samples) {
    return;
  }
  if (!mpu6050_cal_finish(imu)) {
    imu->calibration_error_count++;
    mpu6050_enter_error(imu);
    return;
  }

  imu->state = MPU6050_STATE_READY;
  imu->next_sample_ms = now_ms;
  imu->last_request_failed = false;
}

/**
 * @brief 在 READY 状态下按采样周期读取六轴数据、扣除零偏并提交基类滤波发布。
 * @param[in,out] imu    MPU6050 对象指针。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void mpu6050_process_ready(imu_mpu6050_t *imu, uint32_t now_ms) {
  if (!Sys_Time_HasReachedMs(now_ms, imu->next_sample_ms)) {
    return;
  }
  imu->next_sample_ms = now_ms + imu->sample_period_ms;

  mpu6050_raw_sample_t raw = {0};
  if (mpu6050_acquire(imu, now_ms, &raw) != BSP_OK) {
    imu->last_request_failed = true;
    return;
  }

  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    raw.gyro_dps[axis] -= imu->gyro_bias_dps[axis];
  }

  // 临界区更新原始工程单位快照，供外部诊断接口读取
  Sys_Critical_Enter();
  imu->raw_sample = raw;
  imu->has_raw_sample = true;
  Sys_Critical_Exit();

  filter_sample_t input = {
      .valid_mask = IMU_CHANNEL_ACCEL_X | IMU_CHANNEL_ACCEL_Y | IMU_CHANNEL_ACCEL_Z |
                    IMU_CHANNEL_GYRO_X | IMU_CHANNEL_GYRO_Y | IMU_CHANNEL_GYRO_Z,
      .time_ms = now_ms,
  };
  input.value[IMU_CHANNEL_INDEX_ACCEL_X] = raw.accel_mps2[0];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Y] = raw.accel_mps2[1];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Z] = raw.accel_mps2[2];
  input.value[IMU_CHANNEL_INDEX_GYRO_X] = raw.gyro_dps[0];
  input.value[IMU_CHANNEL_INDEX_GYRO_Y] = raw.gyro_dps[1];
  input.value[IMU_CHANNEL_INDEX_GYRO_Z] = raw.gyro_dps[2];

  if (Dev_IMU_Base_SubmitSample(&imu->base, &input)) {
    imu->last_request_failed = false;
  } else {
    imu->filter_reject_count++;
    imu->last_request_failed = true;
  }
}

/**
 * @brief MPU6050 虚表回调：启动软复位状态机。
 * @param[in,out] imu_h IMU 基类指针（实际指向 imu_mpu6050_t）。
 */
static void mpu6050_start_op(imu_base_t *imu_h) {
  imu_mpu6050_t *imu = (imu_mpu6050_t *)imu_h;
  if (imu == NULL || imu->is_started) {
    return;
  }
  imu->is_started = true;
  imu->state = MPU6050_STATE_RESET_PENDING;
  imu->last_request_failed = false;
}

/**
 * @brief MPU6050 虚表回调：周期推进复位、配置、校准与采样状态机。
 * @param[in,out] imu_h  IMU 基类指针（实际指向 imu_mpu6050_t）。
 * @param[in]     now_ms 当前系统单调时间戳（单位：ms）。
 */
static void mpu6050_process_op(imu_base_t *imu_h, uint32_t now_ms) {
  imu_mpu6050_t *imu = (imu_mpu6050_t *)imu_h;
  if (imu == NULL || !imu->is_started) {
    return;
  }

  switch (imu->state) {
  case MPU6050_STATE_RESET_PENDING:
    if (mpu6050_write_reg(imu, MPU6050_REG_PWR_MGMT_1, MPU6050_PWR1_DEVICE_RESET) != BSP_OK) {
      mpu6050_enter_error(imu);
      return;
    }
    imu->state = MPU6050_STATE_RESET_WAIT;
    imu->state_deadline_ms = now_ms + MPU6050_RESET_DELAY_MS;
    break;

  case MPU6050_STATE_RESET_WAIT:
    if (!Sys_Time_HasReachedMs(now_ms, imu->state_deadline_ms)) {
      return;
    }
    if (mpu6050_configure(imu) != BSP_OK) {
      imu->configuration_error_count++;
      mpu6050_enter_error(imu);
      return;
    }
    imu->state = MPU6050_STATE_CALIBRATE_PENDING;
    break;

  case MPU6050_STATE_CALIBRATE_PENDING:
    mpu6050_cal_reset(imu);
    imu->next_sample_ms = now_ms;
    if (imu->cfg.calibration_samples == 0U) {
      for (uint8_t axis = 0U; axis < 3U; ++axis) {
        imu->gyro_bias_dps[axis] = 0.0f;
      }
      imu->state = MPU6050_STATE_READY;
    } else {
      imu->state = MPU6050_STATE_CALIBRATING;
    }
    break;

  case MPU6050_STATE_CALIBRATING:
    mpu6050_process_calibration(imu, now_ms);
    break;

  case MPU6050_STATE_READY:
    mpu6050_process_ready(imu, now_ms);
    break;

  case MPU6050_STATE_UNINITIALIZED:
  case MPU6050_STATE_ERROR:
  default:
    break;
  }
}

/**
 * @brief MPU6050 虚表回调：汇总设备忙闲状态与饱和累加错误计数。
 * @param[in,out] imu_h  IMU 基类指针（实际指向 imu_mpu6050_t）。
 * @param[out]    status 状态输出结构体指针。
 */
static void mpu6050_get_status_op(imu_base_t *imu_h, imu_status_t *status) {
  imu_mpu6050_t *imu = (imu_mpu6050_t *)imu_h;
  if (imu == NULL || status == NULL) {
    return;
  }

  uint64_t err_sum = (uint64_t)imu->i2c_error_count + (uint64_t)imu->configuration_error_count +
                     (uint64_t)imu->calibration_error_count + (uint64_t)imu->filter_reject_count;
  if (err_sum > UINT32_MAX) {
    err_sum = UINT32_MAX;
  }

  *status = (imu_status_t){
      .is_started = imu->is_started,
      .is_busy = (imu->state == MPU6050_STATE_RESET_PENDING) ||
                 (imu->state == MPU6050_STATE_RESET_WAIT) ||
                 (imu->state == MPU6050_STATE_CALIBRATE_PENDING) ||
                 (imu->state == MPU6050_STATE_CALIBRATING),
      .last_request_failed = imu->last_request_failed,
      .error_count = (uint32_t)err_sum,
  };
}

/**
 * @brief MPU6050 虚表回调：委托基类执行软件偏航归零。
 * @param[in,out] imu_h IMU 基类指针。
 */
static void mpu6050_set_zero_op(imu_base_t *imu_h) { Dev_IMU_Base_ZeroYaw(imu_h); }

/** @brief MPU6050 IMU 多态虚表实例。 */
static const imu_ops_t s_mpu6050_ops = {
    .start = mpu6050_start_op,
    .process = mpu6050_process_op,
    .get_status = mpu6050_get_status_op,
    .set_zero = mpu6050_set_zero_op,
};

/**
 * @brief 校验 MPU6050 初始化配置参数、分频器范围与轴映射排列合法性。
 * @param[in] cfg 待校验的配置结构体指针。
 * @return 配置是否合法。
 */
static bool mpu6050_cfg_isvalid(const imu_mpu6050_cfg_t *cfg) {
  if (cfg == NULL || cfg->name == NULL || cfg->filter == NULL || cfg->i2c.hi2c == NULL ||
      cfg->i2c.timeout_ms == 0U ||
      (cfg->i2c.addr_7bit != MPU6050_ADDRESS_AD0_LOW &&
       cfg->i2c.addr_7bit != MPU6050_ADDRESS_AD0_HIGH) ||
      cfg->dlpf > MPU6050_DLPF_5HZ || cfg->gyro_range > MPU6050_GYRO_RANGE_2000DPS ||
      cfg->accel_range > MPU6050_ACCEL_RANGE_16G || cfg->sample_rate_hz < 4U ||
      cfg->sample_rate_hz > 1000U || cfg->calibration_samples > 1000U) {
    return false;
  }

  uint32_t rate_hz = mpu6050_gyro_rate_hz(cfg->dlpf);
  uint32_t div = (rate_hz + (cfg->sample_rate_hz / 2U)) / cfg->sample_rate_hz;
  div = (div == 0U) ? 0U : (div - 1U);
  if (div > UINT8_MAX) {
    return false;
  }

  // 校验 axis_source 为 {0, 1, 2} 的无重复排列，且 axis_sign 仅为 ±1
  bool seen[3] = {false, false, false};
  for (uint8_t axis = 0U; axis < 3U; ++axis) {
    uint8_t src = cfg->axis_source[axis];
    int8_t sgn = cfg->axis_sign[axis];
    if (src >= 3U || seen[src] || (sgn != -1 && sgn != 1)) {
      return false;
    }
    seen[src] = true;
  }
  return true;
}

bsp_status_t Dev_IMU_MPU6050_Init(imu_mpu6050_t *imu, const imu_mpu6050_cfg_t *cfg) {
  if (imu == NULL || !mpu6050_cfg_isvalid(cfg)) {
    return BSP_ERR_INVALID_ARG;
  }

  (void)memset(imu, 0, sizeof(*imu));
  imu->cfg = *cfg;
  imu->state = MPU6050_STATE_UNINITIALIZED;
  imu->accel_lsb_per_g = mpu6050_accel_lsb(cfg->accel_range);
  imu->gyro_lsb_per_dps = mpu6050_gyro_lsb(cfg->gyro_range);

  uint32_t rate_hz = mpu6050_gyro_rate_hz(cfg->dlpf);
  uint32_t div = (rate_hz + (cfg->sample_rate_hz / 2U)) / cfg->sample_rate_hz;
  div = (div == 0U) ? 0U : (div - 1U);
  imu->sample_rate_divider = (uint8_t)div;

  uint32_t period_ms = (1000U + (cfg->sample_rate_hz / 2U)) / cfg->sample_rate_hz;
  imu->sample_period_ms =
      (period_ms < MPU6050_SAMPLE_PERIOD_MIN_MS) ? MPU6050_SAMPLE_PERIOD_MIN_MS : period_ms;

  uint8_t who_am_i = 0U;
  bsp_status_t st = mpu6050_read_regs(imu, MPU6050_REG_WHO_AM_I, &who_am_i, 1U);
  if (st != BSP_OK) {
    imu->last_request_failed = true;
    return st;
  }
  if (who_am_i != MPU6050_WHO_AM_I_VALUE && who_am_i != MPU6050_WHO_AM_I_ALT) {
    imu->last_request_failed = true;
    return BSP_ERR_HAL;
  }

  Dev_IMU_Base_Init(&imu->base, cfg->name, &s_mpu6050_ops, cfg->filter);
  imu->last_request_failed = false;
  return BSP_OK;
}

bool Dev_IMU_MPU6050_GetRawSample(const imu_mpu6050_t *imu, mpu6050_raw_sample_t *out_sample) {
  if (imu == NULL || out_sample == NULL) {
    return false;
  }
  Sys_Critical_Enter();
  *out_sample = imu->raw_sample;
  bool available = imu->has_raw_sample;
  Sys_Critical_Exit();
  return available;
}

bsp_status_t Dev_IMU_MPU6050_RequestGyroCalibration(imu_mpu6050_t *imu) {
  if (imu == NULL || !imu->is_started || imu->cfg.calibration_samples == 0U ||
      imu->state != MPU6050_STATE_READY) {
    return BSP_ERR_INVALID_ARG;
  }
  imu->last_request_failed = false;
  imu->state = MPU6050_STATE_CALIBRATE_PENDING;
  return BSP_OK;
}

#else

/** @brief I2C 模块未启用时的空编译单元占位类型。 */
typedef int imu_mpu6050_empty_tu_t;

#endif /* HAL_I2C_MODULE_ENABLED || BSP_HOST_STUB */
