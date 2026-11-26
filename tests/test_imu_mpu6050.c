/**
 * @file test_imu_mpu6050.c
 * @brief imu_mpu6050 I2C 探测、配置回读、静止零偏标定与 14 字节突发换算测试。
 * @ingroup tests
 */

#include "filter_passthrough.h"
#include "imu_mpu6050.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_regs[128];
static HAL_StatusTypeDef s_i2c_status = HAL_OK;

static HAL_StatusTypeDef stub_i2c_write(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                        uint16_t mem_addr, uint16_t mem_add_size, uint8_t *data,
                                        uint16_t size, uint32_t timeout_ms) {
  (void)hi2c;
  (void)dev_addr;
  (void)mem_add_size;
  (void)timeout_ms;
  if (s_i2c_status != HAL_OK) {
    return s_i2c_status;
  }
  for (uint16_t i = 0U; i < size; ++i) {
    s_regs[(mem_addr + i) & 0x7FU] = data[i];
  }
  return HAL_OK;
}

static HAL_StatusTypeDef stub_i2c_read(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                       uint16_t mem_addr, uint16_t mem_add_size, uint8_t *data,
                                       uint16_t size, uint32_t timeout_ms) {
  (void)hi2c;
  (void)dev_addr;
  (void)mem_add_size;
  (void)timeout_ms;
  if (s_i2c_status != HAL_OK) {
    return s_i2c_status;
  }
  for (uint16_t i = 0U; i < size; ++i) {
    data[i] = s_regs[(mem_addr + i) & 0x7FU];
  }
  return HAL_OK;
}

static void write_be16(uint8_t reg, int16_t val) {
  s_regs[reg] = (uint8_t)(((uint16_t)val >> 8U) & 0xFFU);
  s_regs[reg + 1U] = (uint8_t)((uint16_t)val & 0xFFU);
}

static void setup_sample(int16_t ax, int16_t ay, int16_t az, int16_t gx, int16_t gy, int16_t gz) {
  write_be16(0x3BU, ax);
  write_be16(0x3DU, ay);
  write_be16(0x3FU, az);
  write_be16(0x41U, 0);
  write_be16(0x43U, gx);
  write_be16(0x45U, gy);
  write_be16(0x47U, gz);
}

static void test_mpu6050_probe_cal_and_burst(void) {
  BSP_Stub_ResetAll();
  (void)memset(s_regs, 0, sizeof(s_regs));
  s_i2c_status = HAL_OK;
  BSP_Stub_I2C_SetHandlers(NULL, stub_i2c_write, stub_i2c_read);

  I2C_HandleTypeDef hi2c = {0};
  filter_passthrough_t pt;
  assert(Filter_Passthrough_Init(&pt, "pt") == FILTER_STATUS_OK);

  imu_mpu6050_cfg_t cfg = {
      .name = "mpu",
      .i2c = {.hi2c = &hi2c, .addr_7bit = MPU6050_ADDRESS_AD0_LOW, .timeout_ms = 20U},
      .filter = &pt.base,
      .dlpf = MPU6050_DLPF_42HZ,
      .gyro_range = MPU6050_GYRO_RANGE_2000DPS,
      .accel_range = MPU6050_ACCEL_RANGE_8G,
      .sample_rate_hz = 200U,
      .calibration_samples = 3U,
      .axis_source = {1U, 0U, 2U},
      .axis_sign = {1, -1, 1},
  };

  /* 错误 WHO_AM_I 被拒绝。 */
  s_regs[0x75U] = 0x12U;
  imu_mpu6050_t imu;
  assert(Dev_IMU_MPU6050_Init(&imu, &cfg) == BSP_ERR_HAL);

  s_regs[0x75U] = MPU6050_WHO_AM_I_VALUE;
  assert(Dev_IMU_MPU6050_Init(&imu, &cfg) == BSP_OK);
  Dev_IMU_Base_Start(&imu.base);

  /* 软复位与 100 ms 等待后配置回读。 */
  Dev_IMU_Base_Process(&imu.base, 0U);
  assert(s_regs[0x6BU] == 0x80U);
  Dev_IMU_Base_Process(&imu.base, 100U);
  Dev_IMU_Base_Process(&imu.base, 101U);
  assert(imu.state == MPU6050_STATE_CALIBRATING);

  /* 3 帧静止采样（az=+1g=4096 LSB，gz=+164 LSB => +10 deg/s 零偏）。 */
  setup_sample(0, 0, 4096, 0, 0, 164);
  Dev_IMU_Base_Process(&imu.base, 102U);
  Dev_IMU_Base_Process(&imu.base, 107U);
  Dev_IMU_Base_Process(&imu.base, 112U);
  assert(imu.state == MPU6050_STATE_READY);

  /* 正式采样：ax_raw=+4096 (映射至 -ay_body), gz_raw=+328 (扣 10 dps 零偏后=+10 dps)。 */
  setup_sample(4096, 0, 4096, 0, 0, 328);
  Dev_IMU_Base_Process(&imu.base, 117U);

  mpu6050_raw_sample_t raw = {0};
  assert(Dev_IMU_MPU6050_GetRawSample(&imu, &raw));
  assert(fabsf(raw.accel_mps2[1] + 9.80665f) < 1e-3f);
  assert(fabsf(Dev_IMU_Base_GetGyroZ(&imu.base) - 10.0f) < 0.05f);
  assert(Dev_IMU_MPU6050_RequestGyroCalibration(&imu) == BSP_OK);

  /* 晃动导致加速度超限时标定进入 ERROR。 */
  setup_sample(0, 0, 16384, 0, 0, 0);
  Dev_IMU_Base_Process(&imu.base, 125U);
  Dev_IMU_Base_Process(&imu.base, 126U);
  assert(imu.state == MPU6050_STATE_ERROR);
}

int main(void) {
  test_mpu6050_probe_cal_and_burst();
  (void)puts("test_imu_mpu6050 passed");
  return 0;
}
