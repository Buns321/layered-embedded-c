//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file mpu6050_i2c.c
 * @brief MPU6050 I2C IMU 驱动实现。
 * @ingroup drivers_concrete
 */

#include "mpu6050_i2c.h"
#include "system_critical.h"
#include "system_time_window.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define MPU6050_REG_SMPLRT_DIV       0x19u
#define MPU6050_REG_CONFIG           0x1Au
#define MPU6050_REG_GYRO_CONFIG      0x1Bu
#define MPU6050_REG_ACCEL_CONFIG     0x1Cu
#define MPU6050_REG_INT_ENABLE       0x38u
#define MPU6050_REG_ACCEL_XOUT_H     0x3Bu
#define MPU6050_REG_PWR_MGMT_1       0x6Bu
#define MPU6050_REG_PWR_MGMT_2       0x6Cu
#define MPU6050_REG_USER_CTRL        0x6Au
#define MPU6050_REG_WHO_AM_I         0x75u

#define MPU6050_PWR1_DEVICE_RESET    0x80u
#define MPU6050_PWR1_CLOCK_PLL_XGYRO 0x01u
#define MPU6050_SAMPLE_PERIOD_MIN_MS 1u
#define MPU6050_RESET_DELAY_MS       100u
#define MPU6050_BURST_SIZE           14u
#define MPU6050_GRAVITY_MPS2         9.80665f
#define MPU6050_CALIBRATION_MAX_STD_DPS 3.0f
#define MPU6050_CALIBRATION_MIN_ACCEL_MPS2 3.0f
#define MPU6050_CALIBRATION_MAX_ACCEL_MPS2 15.0f

/**
 * @brief 推进 MPU6050 复位、配置、校准和采样状态机。
 * @param imu_h mpu6050_i2c_t 内嵌的 IMU 基类视图；不可为 NULL。
 * @param now_ms 单调毫秒时间戳，单位 ms；须与 next_sample_ms 和 state_deadline_ms 使用同一时基。
 * @pre imu_h 已通过 Dev_MPU6050_I2C_Init 初始化，且对象已 Start；只允许单一传感器任务推进。
 * @post 按当前状态可能写寄存器、读取样本、更新校准累计或发布滤波帧；未 Start 或未到截止时间时保持状态。
 * @warning 会执行阻塞 I2C 事务且不是中断安全；不得在 ISR 中或与同一对象的 process() 并发调用。
 */
static void mpu6050_process(imu_base_t *imu_h, uint32_t now_ms);

/**
 * @brief 通过 Platform I2C 写入 MPU6050 的单个 8 位寄存器。
 * @param mpu6050_i2c_h 已绑定 I2C、地址和超时的 MPU6050 对象；不可为 NULL。
 * @param reg 8 位设备寄存器地址，范围 0x00..0xFF。
 * @param value 要写入寄存器的 8 位值，范围 0x00..0xFF。
 * @retval PLATFORM_STATUS_OK 写寄存器事务完成。
 * @retval PLATFORM_STATUS_BUSY I2C 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT i2c_timeout_ms 内事务未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 对象已初始化，对应 I2C 实例和 7 位地址有效，调用方未并发使用同一总线实例。
 * @post 成功时硬件寄存器已提交新值；失败时 i2c_error_count 和 last_request_failed 增加，状态机不改变。
 * @warning 阻塞任务上下文调用，最长等待 config.i2c_timeout_ms；不得在 ISR 中调用。
 */
static platform_status_t mpu6050_write_reg(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint8_t reg,
  uint8_t value
) {
  assert(mpu6050_i2c_h);

  platform_status_t status = Platform_I2C_MemWrite(
    mpu6050_i2c_h->i2c,
    mpu6050_i2c_h->address,
    reg,
    1u,
    &value,
    1u,
    mpu6050_i2c_h->config.i2c_timeout_ms
  );

  if (status != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->i2c_error_count++;
    mpu6050_i2c_h->last_request_failed = true;
  }

  return status;
}

/**
 * @brief 从 MPU6050 起始寄存器连续读取一段字节。
 * @param mpu6050_i2c_h 已绑定 I2C、地址和超时的 MPU6050 对象；不可为 NULL。
 * @param reg 8 位起始设备寄存器地址，范围 0x00..0xFF。
 * @param data 输出缓冲区；不可为 NULL，容量必须至少为 size 字节。
 * @param size 连续读取字节数，范围 1..UINT16_MAX。
 * @retval PLATFORM_STATUS_OK 整段读取事务完成。
 * @retval PLATFORM_STATUS_BUSY I2C 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT i2c_timeout_ms 内事务未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 对象已初始化，data 覆盖 size 字节，调用方未并发使用同一 I2C 实例。
 * @post 成功时 data 的 size 字节全部来自连续寄存器；失败时内容可能部分更新，不得视为有效样本。
 * @warning 阻塞任务上下文调用，最长等待 config.i2c_timeout_ms；不得在 ISR 中调用。
 */
static platform_status_t mpu6050_read_regs(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint8_t reg,
  uint8_t *data,
  uint16_t size
) {
  assert(mpu6050_i2c_h && data && size != 0u);

  platform_status_t status = Platform_I2C_MemRead(
    mpu6050_i2c_h->i2c,
    mpu6050_i2c_h->address,
    reg,
    1u,
    data,
    size,
    mpu6050_i2c_h->config.i2c_timeout_ms
  );

  if (status != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->i2c_error_count++;
    mpu6050_i2c_h->last_request_failed = true;
  }

  return status;
}

/**
 * @brief 通过单字节读事务读取 MPU6050 寄存器。
 * @param mpu6050_i2c_h 已绑定 I2C、地址和超时的 MPU6050 对象；不可为 NULL。
 * @param reg 8 位设备寄存器地址，范围 0x00..0xFF。
 * @param value 单字节输出地址；不可为 NULL，成功时写入寄存器值。
 * @retval PLATFORM_STATUS_OK 单字节读取事务完成。
 * @retval PLATFORM_STATUS_BUSY I2C 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT i2c_timeout_ms 内事务未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 对象已初始化，value 可写，调用方未并发使用同一 I2C 实例。
 * @post 成功时 value 含寄存器值；失败时 value 内容不确定，错误计数已更新。
 * @warning 阻塞任务上下文调用，不得在 ISR 中调用。
 */
static platform_status_t mpu6050_read_reg(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint8_t reg,
  uint8_t *value
) {
  return mpu6050_read_regs(mpu6050_i2c_h, reg, value, 1u);
}

/**
 * @brief 把两个大端字节解释为有符号 16 位整数。
 * @param data 至少可读两个字节的输入缓冲区；data[0] 为高字节，data[1] 为低字节。
 * @return 符号扩展后的有符号 16 位值，范围 -32768..32767。
 * @pre data 非 NULL 且前两个字节在调用期间可读；函数不检查实际缓冲区长度。
 * @post 纯函数；不修改输入、不访问硬件，也不分配内存。
 */
static int16_t mpu6050_be_i16(const uint8_t *data) {
  assert(data);

  int32_t raw = ((int32_t)data[0] << 8) | data[1];
  if ((raw & 0x8000) != 0) {
    raw -= 0x10000;
  }

  return (int16_t)raw;
}

/**
 * @brief 返回 MPU6050 加速度量程对应的灵敏度。
 * @param range 已定义的加速度量程枚举值；不得传入未定义值。
 * @return 灵敏度，单位 LSB/g：2G 为 16384、4G 为 8192、8G 为 4096、16G 为 2048。
 * @pre range 必须为 MPU6050_ACCEL_RANGE_2G..16G 之一；非法值触发 assert。
 * @post 纯函数；不修改配置或硬件，不分配内存。
 */
static float mpu6050_accel_lsb_per_g(mpu6050_accel_range_t range) {
  switch (range) {
    case MPU6050_ACCEL_RANGE_2G:  return 16384.0f;
    case MPU6050_ACCEL_RANGE_4G:  return 8192.0f;
    case MPU6050_ACCEL_RANGE_8G:  return 4096.0f;
    case MPU6050_ACCEL_RANGE_16G: return 2048.0f;
    default:                      assert(0); return 0.0f;
  }
}

/**
 * @brief 返回 MPU6050 陀螺量程对应的灵敏度。
 * @param range 已定义的陀螺量程枚举值；不得传入未定义值。
 * @return 灵敏度，单位 LSB/(deg/s)：250 dps 为 131、500 为 65.5、1000 为 32.8、2000 为 16.4。
 * @pre range 必须为 MPU6050_GYRO_RANGE_250DPS..2000DPS 之一；非法值触发 assert。
 * @post 纯函数；不修改配置或硬件，不分配内存。
 */
static float mpu6050_gyro_lsb_per_dps(mpu6050_gyro_range_t range) {
  switch (range) {
    case MPU6050_GYRO_RANGE_250DPS:  return 131.0f;
    case MPU6050_GYRO_RANGE_500DPS:  return 65.5f;
    case MPU6050_GYRO_RANGE_1000DPS: return 32.8f;
    case MPU6050_GYRO_RANGE_2000DPS: return 16.4f;
    default:                         assert(0); return 0.0f;
  }
}

/**
 * @brief 返回 DLPF 配置对应的陀螺内部输出率。
 * @param dlpf 已定义的 DLPF 枚举值；256 Hz 档对应 8 kHz 内部采样，其余档对应 1 kHz。
 * @return 陀螺输出率，单位 Hz；合法输入只返回 8000 或 1000。
 * @pre dlpf 必须为 mpu6050_dlpf_t 的已定义枚举值。
 * @post 纯函数；不写 CONFIG 寄存器，不修改采样率分频缓存。
 */
static uint32_t mpu6050_gyro_output_rate(mpu6050_dlpf_t dlpf) {
  return dlpf == MPU6050_DLPF_256HZ ? 8000u : 1000u;
}

/**
 * @brief 按 axis_source/axis_sign 把三轴芯片坐标映射到车体坐标。
 * @param config 已通过 mpu6050_config_is_valid 校验的配置；axis_source 为 0..2 的排列，axis_sign 为 ±1。
 * @param input 三元素芯片轴输入向量；单位由调用方保持，函数不做换算。
 * @param output 三元素车体轴输出缓冲区；单位与 input 相同。
 * @pre config、input、output 均非 NULL；三个数组均可读写至少 3 个 float。
 * @post output[axis] = input[axis_source[axis]] * axis_sign[axis]；input 和 config 不被修改。
 * @warning 违反 axis_source/axis_sign 前置条件会触发 assert；本函数不检查浮点有限性。
 */
static void mpu6050_apply_axis_map(
  const mpu6050_config_t *config,
  const float input[3],
  float output[3]
) {
  assert(config && input && output);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    uint8_t source = config->axis_source[axis];
    assert(source < 3u);
    assert(config->axis_sign[axis] == -1 || config->axis_sign[axis] == 1);
    output[axis] = input[source] * (float)config->axis_sign[axis];
  }
}

/**
 * @brief 在系统临界区内发布最近一次未滤波样本。
 * @param mpu6050_i2c_h 已初始化的 MPU6050 对象；不可为 NULL，作为唯一的传感器任务写入方。
 * @param sample 已换算并完成轴映射的只读样本；不可为 NULL，内容按值复制。
 * @pre 对象已初始化，sample 指向完整 mpu6050_raw_sample_t，调用方已完成零偏处理。
 * @post raw_sample 被整体覆盖，has_raw_sample 置为 true；不调用滤波管线、不访问 I2C。
 * @note 临界区只保护结构体复制和标志更新；读取方应通过 GetRawSample 获取一致副本。
 */
static void mpu6050_store_raw(
  mpu6050_i2c_t *mpu6050_i2c_h,
  const mpu6050_raw_sample_t *sample
) {
  assert(mpu6050_i2c_h && sample);

  Sys_Critical_Enter();
  mpu6050_i2c_h->raw_sample = *sample;
  mpu6050_i2c_h->has_raw_sample = true;
  Sys_Critical_Exit();
}

/**
 * @brief 把 MPU6050 状态机置为终态 ERROR 并标记最近请求失败。
 * @param mpu6050_i2c_h 已初始化的 MPU6050 对象；不可为 NULL。
 * @pre 调用方已在相应错误计数器中记录本次失败原因。
 * @post state 变为 MPU6050_STATE_ERROR，last_request_failed 置为 true；不清零样本或计数器。
 * @note 不访问硬件、不分配内存；process() 在 ERROR 状态直接返回，恢复需要重新 Init/Start。
 */
static void mpu6050_enter_error(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(mpu6050_i2c_h);

  mpu6050_i2c_h->state = MPU6050_STATE_ERROR;
  mpu6050_i2c_h->last_request_failed = true;
}

/**
 * @brief 读取一帧原始六轴数据并换算为工程单位。
 * @param mpu6050_i2c_h 已配置的 MPU6050 对象；不可为 NULL。
 * @param now_ms 本次采样发起时刻的单调毫秒时间戳，单位 ms，原样写入样本。
 * @param sample 输出样本；不可为 NULL，仅在读取和换算全部成功后完整写入。
 * @retval PLATFORM_STATUS_OK 14 字节读取成功，accel/gyro/temperature/time 已写入。
 * @retval PLATFORM_STATUS_BUSY I2C 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT i2c_timeout_ms 内读取未完成。
 * @retval PLATFORM_STATUS_ERROR 从设备 NACK、总线错误或其他 HAL 执行失败。
 * @pre 对象已完成 Init 和寄存器配置，派生灵敏度有效，调用方未并发使用同一 I2C 实例。
 * @post 成功时输出 accel_mps2、陀螺 deg/s、温度 °C 和采样时间；不扣零偏、不发布 raw_sample。
 * @warning 从 0x3B 起突发读取 14 字节，是阻塞 I2C 事务；只能在任务上下文调用。
 */
static platform_status_t mpu6050_acquire(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint32_t now_ms,
  mpu6050_raw_sample_t *sample
) {
  assert(mpu6050_i2c_h && sample);

  uint8_t data[MPU6050_BURST_SIZE] = {0};
  platform_status_t status = mpu6050_read_regs(
    mpu6050_i2c_h,
    MPU6050_REG_ACCEL_XOUT_H,
    data,
    MPU6050_BURST_SIZE
  );
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }

  float accel_g[3] = {
    (float)mpu6050_be_i16(&data[0]) / mpu6050_i2c_h->accel_lsb_per_g,
    (float)mpu6050_be_i16(&data[2]) / mpu6050_i2c_h->accel_lsb_per_g,
    (float)mpu6050_be_i16(&data[4]) / mpu6050_i2c_h->accel_lsb_per_g,
  };
  float gyro_dps[3] = {
    (float)mpu6050_be_i16(&data[8]) / mpu6050_i2c_h->gyro_lsb_per_dps,
    (float)mpu6050_be_i16(&data[10]) / mpu6050_i2c_h->gyro_lsb_per_dps,
    (float)mpu6050_be_i16(&data[12]) / mpu6050_i2c_h->gyro_lsb_per_dps,
  };

  float mapped_accel_g[3] = {0};
  float mapped_gyro_dps[3] = {0};
  mpu6050_apply_axis_map(&mpu6050_i2c_h->config, accel_g, mapped_accel_g);
  mpu6050_apply_axis_map(&mpu6050_i2c_h->config, gyro_dps, mapped_gyro_dps);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    sample->accel_mps2[axis] =
      mapped_accel_g[axis] * MPU6050_GRAVITY_MPS2;
    sample->gyro_dps[axis] = mapped_gyro_dps[axis];
  }

  int16_t temperature_raw = mpu6050_be_i16(&data[6]);
  sample->temperature_c =
    (float)temperature_raw / 340.0f + 36.53f;
  sample->time_ms = now_ms;

  return PLATFORM_STATUS_OK;
}

/**
 * @brief 判断样本加速度模长是否处于静止标定窗口。
 * @param sample 已换算为车体轴、单位 m/s² 的只读样本；不可为 NULL。
 * @retval true 三轴模长落在 [3.0, 15.0] m/s² 闭区间。
 * @retval false 模长低于 3.0 或高于 15.0 m/s²，包括传感器饱和或明显运动。
 * @pre sample 的三个 accel_mps2 元素均已完成轴映射和单位换算。
 * @post 纯函数；不修改样本或校准累计，不访问硬件。
 * @warning 该判据只检查加速度模长，不检查陀螺方差；方差在 calibration_finish 中检查。
 */
static bool mpu6050_accel_is_stationary(
  const mpu6050_raw_sample_t *sample
) {
  assert(sample);

  float norm_sq = 0.0f;
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    norm_sq += sample->accel_mps2[axis] * sample->accel_mps2[axis];
  }

  float norm = sqrtf(norm_sq);
  return
    norm >= MPU6050_CALIBRATION_MIN_ACCEL_MPS2 &&
    norm <= MPU6050_CALIBRATION_MAX_ACCEL_MPS2;
}

/**
 * @brief 清零一次新标定所需的样本计数和三轴累计量。
 * @param mpu6050_i2c_h 已初始化的 MPU6050 对象；不可为 NULL。
 * @pre 调用方已决定开始新的陀螺零偏采集，且没有并发采样。
 * @post calibration_count、calibration_gyro_sum 和 calibration_gyro_sum_sq 全部清零。
 * @note 不修改已有 gyro_bias_dps，也不访问硬件；新零偏只在 calibration_finish 成功时覆盖。
 */
static void mpu6050_calibration_reset(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(mpu6050_i2c_h);

  mpu6050_i2c_h->calibration_count = 0u;

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    mpu6050_i2c_h->calibration_gyro_sum[axis] = 0.0f;
    mpu6050_i2c_h->calibration_gyro_sum_sq[axis] = 0.0f;
  }
}

/**
 * @brief 根据累计样本计算陀螺零偏并检查三轴标准差。
 * @param mpu6050_i2c_h 已累积完 calibration_samples 个样本的对象；不可为 NULL。
 * @retval true 三轴总体标准差均不超过 3.0 deg/s，gyro_bias_dps 已更新为三轴均值。
 * @retval false 任一轴标准差超过 3.0 deg/s；之前已处理的轴可能已写入新均值。
 * @pre calibration_count 必须等于 config.calibration_samples 且大于 0；只允许单一采样任务调用。
 * @post 成功时零偏为本次静止样本均值；失败时对象可能含部分更新，调用方应进入 ERROR 并重新标定。
 * @warning 只计算陀螺标准差，不验证加速度模长；静止判据必须在累计前完成。
 */
static bool mpu6050_calibration_finish(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(
    mpu6050_i2c_h &&
    mpu6050_i2c_h->calibration_count == mpu6050_i2c_h->config.calibration_samples
  );

  float count = (float)mpu6050_i2c_h->calibration_count;

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    float mean = mpu6050_i2c_h->calibration_gyro_sum[axis] / count;
    float variance =
      mpu6050_i2c_h->calibration_gyro_sum_sq[axis] / count - mean * mean;
    if (variance < 0.0f) {
      variance = 0.0f;
    }

    if (sqrtf(variance) > MPU6050_CALIBRATION_MAX_STD_DPS) {
      return false;
    }

    mpu6050_i2c_h->gyro_bias_dps[axis] = mean;
  }

  return true;
}

/**
 * @brief 把一帧静止陀螺样本加入零偏均值和方差累计。
 * @param mpu6050_i2c_h 已初始化且正在标定的 MPU6050 对象；不可为 NULL。
 * @param sample 已通过静止判据的只读样本；使用 gyro_dps，单位 deg/s。
 * @pre calibration_count 小于 config.calibration_samples，sample 的三轴值已完成轴映射。
 * @post 三轴 sum 和 sum_sq 按样本累加，calibration_count 加一；不更新 gyro_bias_dps。
 * @note 不做范围检查、不访问 I2C；浮点累计误差在有限样本数内可接受。
 */
static void mpu6050_calibration_accumulate(
  mpu6050_i2c_t *mpu6050_i2c_h,
  const mpu6050_raw_sample_t *sample
) {
  assert(mpu6050_i2c_h && sample);

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    mpu6050_i2c_h->calibration_gyro_sum[axis] += sample->gyro_dps[axis];
    mpu6050_i2c_h->calibration_gyro_sum_sq[axis] +=
      sample->gyro_dps[axis] * sample->gyro_dps[axis];
  }

  mpu6050_i2c_h->calibration_count++;
}

/**
 * @brief 写入并回读校验 MPU6050 的初始化寄存器序列。
 * @param mpu6050_i2c_h 已完成软复位等待且配置已绑定的对象；不可为 NULL。
 * @retval PLATFORM_STATUS_OK 电源时钟、采样分频、DLPF、量程和使能寄存器均写入且回读匹配。
 * @retval PLATFORM_STATUS_BUSY I2C 实例或总线正忙。
 * @retval PLATFORM_STATUS_TIMEOUT 任一写或回读事务在 i2c_timeout_ms 内未完成。
 * @retval PLATFORM_STATUS_ERROR I2C 执行失败，或关键字段回读与配置不匹配。
 * @pre 设备软复位已等待至少 100 ms，config 及派生分频/灵敏度字段有效，总线由当前任务独占。
 * @post 成功时陀螺和加速度计已使能、外部中断/FIFO/DMP 关闭；失败时寄存器可能只更新了一部分。
 * @warning 包含多次阻塞 I2C 事务；调用方必须保持单一写者，失败后应进入 ERROR 而不是继续采样。
 */
static platform_status_t mpu6050_configure(mpu6050_i2c_t *mpu6050_i2c_h) {
  assert(mpu6050_i2c_h);

  const uint8_t gyro_config =
    (uint8_t)((uint8_t)mpu6050_i2c_h->config.gyro_range << 3u);
  const uint8_t accel_config =
    (uint8_t)((uint8_t)mpu6050_i2c_h->config.accel_range << 3u);

  platform_status_t status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_PWR_MGMT_1,
    MPU6050_PWR1_CLOCK_PLL_XGYRO
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_SMPLRT_DIV,
    mpu6050_i2c_h->sample_rate_divider
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_CONFIG,
    (uint8_t)mpu6050_i2c_h->config.dlpf
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_GYRO_CONFIG,
    gyro_config
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(
    mpu6050_i2c_h,
    MPU6050_REG_ACCEL_CONFIG,
    accel_config
  );
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(mpu6050_i2c_h, MPU6050_REG_INT_ENABLE, 0u);
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(mpu6050_i2c_h, MPU6050_REG_USER_CTRL, 0u);
  if (status != PLATFORM_STATUS_OK) return status;

  status = mpu6050_write_reg(mpu6050_i2c_h, MPU6050_REG_PWR_MGMT_2, 0u);
  if (status != PLATFORM_STATUS_OK) return status;

  uint8_t readback = 0u;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_SMPLRT_DIV, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if (readback != mpu6050_i2c_h->sample_rate_divider) return PLATFORM_STATUS_ERROR;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_CONFIG, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x07u) != (uint8_t)mpu6050_i2c_h->config.dlpf) {
    return PLATFORM_STATUS_ERROR;
  }

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_GYRO_CONFIG, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x18u) != gyro_config) return PLATFORM_STATUS_ERROR;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_ACCEL_CONFIG, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x18u) != accel_config) return PLATFORM_STATUS_ERROR;

  status = mpu6050_read_reg(mpu6050_i2c_h, MPU6050_REG_PWR_MGMT_1, &readback);
  if (status != PLATFORM_STATUS_OK) return status;
  if ((readback & 0x7Fu) != MPU6050_PWR1_CLOCK_PLL_XGYRO) {
    return PLATFORM_STATUS_ERROR;
  }

  return PLATFORM_STATUS_OK;
}

/**
 * @brief 标记 MPU6050 已启动并请求下一次 process 执行软复位。
 * @param imu_h mpu6050_i2c_t 内嵌的 IMU 基类视图；不可为 NULL。
 * @pre 对象已 Init，且 is_started 为 false；只允许单一传感器任务调用。
 * @post is_started 置为 true，state 进入 RESET_PENDING，last_request_failed 清零。
 * @note 不访问 I2C；软复位、配置和校准由后续 process() 异步推进。
 */
static void mpu6050_start(imu_base_t *imu_h) {
  assert(imu_h);
  mpu6050_i2c_t *mpu6050_i2c_h = (mpu6050_i2c_t *)imu_h;

  assert(!mpu6050_i2c_h->is_started);

  mpu6050_i2c_h->is_started = true;
  mpu6050_i2c_h->state = MPU6050_STATE_RESET_PENDING;
  mpu6050_i2c_h->last_request_failed = false;
}

/**
 * @brief 汇总 MPU6050 启动、忙、失败和累计错误计数。
 * @param imu_h mpu6050_i2c_t 内嵌的 IMU 基类视图；不可为 NULL。
 * @param status 输出状态结构；不可为 NULL，函数整体覆盖。
 * @pre 对象已 Init；读取方不得同时销毁或重新 Init 同一对象。
 * @post is_started、is_busy、last_request_failed 和饱和到 UINT32_MAX 的 error_count 已写入。
 * @note 不访问硬件、不分配内存；各计数字段为 volatile 但组合读取不加锁，结果是尽力而为快照。
 */
static void mpu6050_get_status(
  imu_base_t *imu_h,
  imu_status_t *status
) {
  assert(imu_h && status);
  mpu6050_i2c_t *mpu6050_i2c_h = (mpu6050_i2c_t *)imu_h;

  uint64_t error_count =
    (uint64_t)mpu6050_i2c_h->i2c_error_count +
    (uint64_t)mpu6050_i2c_h->configuration_error_count +
    (uint64_t)mpu6050_i2c_h->calibration_error_count +
    (uint64_t)mpu6050_i2c_h->filter_reject_count;

  if (error_count > UINT32_MAX) {
    error_count = UINT32_MAX;
  }

  *status = (imu_status_t){
    .is_started = mpu6050_i2c_h->is_started,
    .is_busy =
      mpu6050_i2c_h->state == MPU6050_STATE_RESET_PENDING ||
      mpu6050_i2c_h->state == MPU6050_STATE_RESET_WAIT ||
      mpu6050_i2c_h->state == MPU6050_STATE_CALIBRATE_PENDING ||
      mpu6050_i2c_h->state == MPU6050_STATE_CALIBRATING,
    .last_request_failed = mpu6050_i2c_h->last_request_failed,
    .error_count = (uint32_t)error_count,
  };
}

/**
 * @brief 把当前滤波姿态的 yaw 设为对外参考零点。
 * @param imu_h mpu6050_i2c_t 内嵌的 IMU 基类视图；不可为 NULL。
 * @pre IMU 基类已 Init；调用方接受当前 yaw 作为新零点，并避免与样本发布并发。
 * @post 委托 Dev_IMU_Base_ZeroYaw，仅改变对外 yaw 偏移，不修改 raw_sample、滤波器或 MPU 寄存器。
 * @note 不访问 I2C、不分配内存；该操作不提供绝对航向，只改变相对零点。
 */
static void mpu6050_set_zero(imu_base_t *imu_h) {
  assert(imu_h);
  Dev_IMU_Base_ZeroYaw(imu_h);
}

static const imu_ops_t mpu6050_imu_ops = {
  .start      = mpu6050_start,
  .process    = mpu6050_process,
  .get_status = mpu6050_get_status,
  .set_zero   = mpu6050_set_zero,
};

/**
 * @brief 检查 MPU6050 配置的地址、枚举、采样率、轴映射和分频范围。
 * @param config 待校验配置；不可为 NULL。
 * @retval true address 为 0x68/0x69，枚举合法，sample_rate_hz 为 4..1000，calibration_samples 为 0..1000，
 *         i2c_timeout_ms 非零，SMPLRT_DIV 可放入 8 位，axis_source/axis_sign 构成有效三轴映射。
 * @retval false 任一字段越界、轴来源重复或符号不是 ±1。
 * @pre config 指向的对象在调用期间可读。
 * @post 纯校验；不修改配置、不访问 I2C，也不写错误计数器。
 * @note 只校验软件可判定范围，不能证明对应 I2C 实例已在 CubeMX 中初始化。
 */
static bool mpu6050_config_is_valid(const mpu6050_config_t *config) {
  assert(config);

  if (
    (config->address != MPU6050_ADDRESS_AD0_LOW &&
     config->address != MPU6050_ADDRESS_AD0_HIGH) ||
    config->dlpf > MPU6050_DLPF_5HZ ||
    config->gyro_range > MPU6050_GYRO_RANGE_2000DPS ||
    config->accel_range > MPU6050_ACCEL_RANGE_16G ||
    config->sample_rate_hz < 4u ||
    config->sample_rate_hz > 1000u ||
    config->calibration_samples > 1000u ||
    config->i2c_timeout_ms == 0u
  ) {
    return false;
  }

  uint32_t output_rate = mpu6050_gyro_output_rate(config->dlpf);
  uint32_t divider =
    (output_rate + config->sample_rate_hz / 2u) /
    config->sample_rate_hz;
  divider = divider == 0u ? 0u : divider - 1u;
  if (divider > UINT8_MAX) {
    return false;
  }

  bool source_seen[3] = {false, false, false};
  for (uint8_t axis = 0u; axis < 3u; axis++) {
    uint8_t source = config->axis_source[axis];
    int8_t sign = config->axis_sign[axis];
    if (source >= 3u || source_seen[source]) {
      return false;
    }
    if (sign != -1 && sign != 1) {
      return false;
    }
    source_seen[source] = true;
  }

  return true;
}

/**
 * @brief 复制已校验配置并缓存 MPU6050 派生运行参数。
 * @param mpu6050_i2c_h 已清零、尚未启动的 MPU6050 对象；不可为 NULL。
 * @param config 已通过 mpu6050_config_is_valid 的配置；不可为 NULL，内容按值复制。
 * @pre 对象未被并发使用，config 的三个轴映射和采样率满足校验函数契约。
 * @post config 被整体复制，i2c/address 已缓存，灵敏度、SMPLRT_DIV 和四舍五入后的采样周期已计算。
 * @note 不访问 I2C、不启动设备；sample_period_ms 最小为 1 ms。
 */
static void mpu6050_bind_config(
  mpu6050_i2c_t *mpu6050_i2c_h,
  const mpu6050_config_t *config
) {
  assert(mpu6050_i2c_h && config);

  mpu6050_i2c_h->config = *config;
  mpu6050_i2c_h->i2c = config->i2c;
  mpu6050_i2c_h->address = config->address;
  mpu6050_i2c_h->accel_lsb_per_g =
    mpu6050_accel_lsb_per_g(config->accel_range);
  mpu6050_i2c_h->gyro_lsb_per_dps =
    mpu6050_gyro_lsb_per_dps(config->gyro_range);

  uint32_t output_rate = mpu6050_gyro_output_rate(config->dlpf);
  uint32_t divider =
    (output_rate + config->sample_rate_hz / 2u) /
    config->sample_rate_hz;
  divider = divider == 0u ? 0u : divider - 1u;
  mpu6050_i2c_h->sample_rate_divider = (uint8_t)divider;

  uint32_t period_ms =
    (1000u + config->sample_rate_hz / 2u) /
    config->sample_rate_hz;
  if (period_ms < MPU6050_SAMPLE_PERIOD_MIN_MS) {
    period_ms = MPU6050_SAMPLE_PERIOD_MIN_MS;
  }
  mpu6050_i2c_h->sample_period_ms = period_ms;
}

/**
 * @brief 初始化并探测 MPU6050。
 * @param imu_h 具体对象；调用方静态分配，函数会清零全部字段。
 * @param name 调试名称；只保存指针，其生命周期必须覆盖对象。
 * @param config 初始化配置；函数整体复制，调用返回后归调用方所有。
 * @param filter_h 已初始化的静态滤波器根节点；只保存非 owning 指针。
 * @retval PLATFORM_STATUS_OK 配置合法且 WHO_AM_I 为 0x68 或 0x34。
 * @retval PLATFORM_STATUS_ERROR 配置非法、通信失败或设备 ID 不匹配。
 * @retval PLATFORM_STATUS_TIMEOUT 探测读取超时。
 * @pre 四个参数非空；对应 I2C 实例已由 CubeMX 完成初始化；对象未被并发使用。
 * @post 成功时 config 已复制、派生字段已计算、imu_base 已绑定 ops/filter；state 为 UNINITIALIZED 且未启动。
 * @note 本函数执行一次阻塞 WHO_AM_I 读取，不软复位、不配置寄存器、不采样、不注册 I2C 回调。
 * @note 失败时对象可能已部分清零或记录 i2c_error_count，调用方应丢弃该对象并重新 Init。
 * @warning 只能在任务上下文、I2C 总线空闲时调用；不得在 ISR 中调用。
 */
platform_status_t Dev_MPU6050_I2C_Init(
  mpu6050_i2c_t *imu_h,
  const char *name,
  const mpu6050_config_t *config,
  filter_base_t *filter_h
) {
  assert(imu_h && name && config && filter_h);

  if (!mpu6050_config_is_valid(config)) {
    return PLATFORM_STATUS_ERROR;
  }

  memset(imu_h, 0, sizeof(*imu_h));
  mpu6050_bind_config(imu_h, config);
  imu_h->state = MPU6050_STATE_UNINITIALIZED;

  uint8_t who_am_i = 0u;
  platform_status_t status =
    mpu6050_read_reg(imu_h, MPU6050_REG_WHO_AM_I, &who_am_i);

  if (status != PLATFORM_STATUS_OK) {
    imu_h->last_request_failed = true;
    return status;
  }

  if (
    who_am_i != MPU6050_WHO_AM_I_VALUE &&
    who_am_i != MPU6050_WHO_AM_I_FIELD
  ) {
    imu_h->last_request_failed = true;
    return PLATFORM_STATUS_ERROR;
  }

  Dev_IMU_Base_Init(
    &imu_h->imu_base,
    name,
    &mpu6050_imu_ops,
    filter_h
  );

  imu_h->last_request_failed = false;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 复制最近一次经单位换算和轴映射、但未经软件滤波的样本。
 * @param imu_h 已 Init 的对象；允许其他任务读取。
 * @param sample 输出缓冲区；调用方拥有，函数在临界区内完整覆盖，即使返回 false 也会写入当前内容。
 * @return has_raw_sample 为 true 时返回 true；false 表示尚未采到样本。
 * @pre imu_h、sample 非空；sample 的生命周期覆盖调用。
 * @post sample 含车体轴 accel_mps2、扣除零偏后的 gyro_dps、temperature_c 和时间戳。
 * @note 可在任意任务调用，不得在 ISR 中调用；临界区只复制结构体，不访问 I2C。
 * @warning 该快照不经过统一滤波管线，不能替代 Dev_IMU_Base_GetSample。
 */
bool Dev_MPU6050_I2C_GetRawSample(
  mpu6050_i2c_t *imu_h,
  mpu6050_raw_sample_t *sample
) {
  assert(imu_h && sample);

  Sys_Critical_Enter();
  *sample = imu_h->raw_sample;
  bool available = imu_h->has_raw_sample;
  Sys_Critical_Exit();

  return available;
}

/**
 * @brief 请求重新采集静止陀螺零偏。
 * @param imu_h 已 Start 且已进入 READY 的对象。
 * @retval PLATFORM_STATUS_OK 已切换到 CALIBRATE_PENDING，下一次 process 开始清空并重采零偏。
 * @retval PLATFORM_STATUS_ERROR 未启动、calibration_samples 为 0，或当前状态不是 READY。
 * @pre imu_h 非空；只允许单一传感器任务调用，不得与 process 并发。
 * @post 成功时 last_request_failed 清零；旧零偏在新标定完成前仍被保留，但期间不发布样本。
 * @note 非阻塞；设备必须保持静止，否则加速度模长或角速度方差检查失败并进入 ERROR。
 * @warning 进入 ERROR 后本函数不再接受请求；零偏只在标定成功时原子更新。
 */
platform_status_t Dev_MPU6050_I2C_RequestGyroCalibration(
  mpu6050_i2c_t *imu_h
) {
  assert(imu_h);

  if (
    !imu_h->is_started ||
    imu_h->config.calibration_samples == 0u ||
    imu_h->state != MPU6050_STATE_READY
  ) {
    return PLATFORM_STATUS_ERROR;
  }

  imu_h->last_request_failed = false;
  imu_h->state = MPU6050_STATE_CALIBRATE_PENDING;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 按采样周期推进一次静止陀螺零偏采集。
 * @param mpu6050_i2c_h 处于 CALIBRATING 状态的 MPU6050 对象；不可为 NULL。
 * @param now_ms 单调毫秒时间戳，单位 ms；用于判断 next_sample_ms 是否到达。
 * @pre 对象已 Start 且状态为 CALIBRATING，调用方为唯一传感器任务，设备保持静止。
 * @post 未到采样时刻时不变；采样后累计 sum/平方和，完成全部样本且标准差合格时进入 READY。
 * @note 读取失败、加速度模长越界或陀螺方差超限都会增加 calibration_error_count 并进入 ERROR。
 * @warning 标定期间不发布 raw_sample 或滤波样本；每次读取都是阻塞 I2C 事务。
 */
static void mpu6050_process_calibration(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint32_t now_ms
) {
  assert(mpu6050_i2c_h);

  if (!Sys_Time_HasReachedMs(now_ms, mpu6050_i2c_h->next_sample_ms)) {
    return;
  }

  mpu6050_i2c_h->next_sample_ms = now_ms + mpu6050_i2c_h->sample_period_ms;

  mpu6050_raw_sample_t sample = {0};
  if (mpu6050_acquire(mpu6050_i2c_h, now_ms, &sample) != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->calibration_error_count++;
    mpu6050_enter_error(mpu6050_i2c_h);
    return;
  }

  if (!mpu6050_accel_is_stationary(&sample)) {
    mpu6050_i2c_h->calibration_error_count++;
    mpu6050_enter_error(mpu6050_i2c_h);
    return;
  }

  mpu6050_calibration_accumulate(mpu6050_i2c_h, &sample);

  if (mpu6050_i2c_h->calibration_count < mpu6050_i2c_h->config.calibration_samples) {
    return;
  }

  if (!mpu6050_calibration_finish(mpu6050_i2c_h)) {
    mpu6050_i2c_h->calibration_error_count++;
    mpu6050_enter_error(mpu6050_i2c_h);
    return;
  }

  mpu6050_i2c_h->state = MPU6050_STATE_READY;
  mpu6050_i2c_h->next_sample_ms = now_ms;
  mpu6050_i2c_h->last_request_failed = false;
}

/**
 * @brief 在 READY 状态按周期采样、扣零偏并向滤波管线提交六轴数据。
 * @param mpu6050_i2c_h 处于 READY 状态的 MPU6050 对象；不可为 NULL。
 * @param now_ms 单调毫秒时间戳，单位 ms；同时作为样本时间和 filter_sample_t.time_ms。
 * @pre 对象已完成标定并处于 READY，调用方为唯一传感器任务，I2C 总线空闲。
 * @post 到采样时刻时 raw_sample 已更新，六轴 accel/gyro 通道提交给 imu_base；成功时清除失败标志。
 * @note 读取失败只置 last_request_failed，不自动进入 ERROR；滤波拒绝会增加 filter_reject_count。
 * @warning 每次采样是阻塞 I2C 事务；不得在 ISR 中调用，也不得与其他写者并发。
 */
static void mpu6050_process_ready(
  mpu6050_i2c_t *mpu6050_i2c_h,
  uint32_t now_ms
) {
  assert(mpu6050_i2c_h);

  if (!Sys_Time_HasReachedMs(now_ms, mpu6050_i2c_h->next_sample_ms)) {
    return;
  }

  mpu6050_i2c_h->next_sample_ms = now_ms + mpu6050_i2c_h->sample_period_ms;

  mpu6050_raw_sample_t raw = {0};
  if (mpu6050_acquire(mpu6050_i2c_h, now_ms, &raw) != PLATFORM_STATUS_OK) {
    mpu6050_i2c_h->last_request_failed = true;
    return;
  }

  for (uint8_t axis = 0u; axis < 3u; axis++) {
    raw.gyro_dps[axis] -= mpu6050_i2c_h->gyro_bias_dps[axis];
  }

  mpu6050_store_raw(mpu6050_i2c_h, &raw);

  filter_sample_t input = {
    .valid_mask =
      IMU_CHANNEL_ACCEL_X |
      IMU_CHANNEL_ACCEL_Y |
      IMU_CHANNEL_ACCEL_Z |
      IMU_CHANNEL_GYRO_X |
      IMU_CHANNEL_GYRO_Y |
      IMU_CHANNEL_GYRO_Z,
    .time_ms = now_ms,
  };

  input.value[IMU_CHANNEL_INDEX_ACCEL_X] = raw.accel_mps2[0];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Y] = raw.accel_mps2[1];
  input.value[IMU_CHANNEL_INDEX_ACCEL_Z] = raw.accel_mps2[2];
  input.value[IMU_CHANNEL_INDEX_GYRO_X] = raw.gyro_dps[0];
  input.value[IMU_CHANNEL_INDEX_GYRO_Y] = raw.gyro_dps[1];
  input.value[IMU_CHANNEL_INDEX_GYRO_Z] = raw.gyro_dps[2];

  if (Dev_IMU_Base_SubmitSample(&mpu6050_i2c_h->imu_base, &input)) {
    mpu6050_i2c_h->last_request_failed = false;
  } else {
    mpu6050_i2c_h->filter_reject_count++;
    mpu6050_i2c_h->last_request_failed = true;
  }
}

/**
 * @brief 推进 MPU6050 复位、配置、校准和采样状态机。
 * @param imu_h mpu6050_i2c_t 内嵌的 IMU 基类视图；不可为 NULL。
 * @param now_ms 单调毫秒时间戳，单位 ms；须与 next_sample_ms 和 state_deadline_ms 使用同一时基。
 * @pre imu_h 已通过 Dev_MPU6050_I2C_Init 初始化，且对象已 Start；只允许单一传感器任务推进。
 * @post 按当前状态可能写寄存器、读取样本、更新校准累计或发布滤波帧；未 Start 或未到截止时间时保持状态。
 * @warning 会执行阻塞 I2C 事务且不是中断安全；不得在 ISR 中或与同一对象的 process() 并发调用。
 */
static void mpu6050_process(imu_base_t *imu_h, uint32_t now_ms) {
  assert(imu_h);
  mpu6050_i2c_t *mpu6050_i2c_h = (mpu6050_i2c_t *)imu_h;

  if (!mpu6050_i2c_h->is_started) {
    return;
  }

  switch (mpu6050_i2c_h->state) {
    case MPU6050_STATE_RESET_PENDING:
      if (
        mpu6050_write_reg(
          mpu6050_i2c_h,
          MPU6050_REG_PWR_MGMT_1,
          MPU6050_PWR1_DEVICE_RESET
        ) != PLATFORM_STATUS_OK
      ) {
        mpu6050_enter_error(mpu6050_i2c_h);
        return;
      }
      mpu6050_i2c_h->state = MPU6050_STATE_RESET_WAIT;
      mpu6050_i2c_h->state_deadline_ms = now_ms + MPU6050_RESET_DELAY_MS;
      break;

    case MPU6050_STATE_RESET_WAIT:
      if (!Sys_Time_HasReachedMs(now_ms, mpu6050_i2c_h->state_deadline_ms)) {
        return;
      }
      if (mpu6050_configure(mpu6050_i2c_h) != PLATFORM_STATUS_OK) {
        mpu6050_i2c_h->configuration_error_count++;
        mpu6050_i2c_h->last_request_failed = true;
        mpu6050_enter_error(mpu6050_i2c_h);
        return;
      }
      mpu6050_i2c_h->state = MPU6050_STATE_CALIBRATE_PENDING;
      break;

    case MPU6050_STATE_CALIBRATE_PENDING:
      mpu6050_calibration_reset(mpu6050_i2c_h);
      mpu6050_i2c_h->next_sample_ms = now_ms;
      if (mpu6050_i2c_h->config.calibration_samples == 0u) {
        for (uint8_t axis = 0u; axis < 3u; axis++) {
          mpu6050_i2c_h->gyro_bias_dps[axis] = 0.0f;
        }
        mpu6050_i2c_h->state = MPU6050_STATE_READY;
      } else {
        mpu6050_i2c_h->state = MPU6050_STATE_CALIBRATING;
      }
      break;

    case MPU6050_STATE_CALIBRATING:
      mpu6050_process_calibration(mpu6050_i2c_h, now_ms);
      break;

    case MPU6050_STATE_READY:
      mpu6050_process_ready(mpu6050_i2c_h, now_ms);
      break;

    case MPU6050_STATE_UNINITIALIZED:
    case MPU6050_STATE_ERROR:
    default:
      return;
  }
}
