//
// 由 Buns_ 创建于 2026/10/23。
//

/**
 * @file mpu6050_i2c.h
 * @brief MPU6050 I2C 驱动、原始样本与统一 IMU 滤波管线适配。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_MPU6050_I2C_H
#define LAYERED_EMBEDDED_C_MPU6050_I2C_H

#include "imu_base.h"
#include "platform_i2c.h"
#include <stdbool.h>
#include <stdint.h>

#define MPU6050_ADDRESS_AD0_LOW    0x68u
#define MPU6050_ADDRESS_AD0_HIGH   0x69u
#define MPU6050_WHO_AM_I_VALUE     0x68u
#define MPU6050_WHO_AM_I_FIELD     0x34u

/**
 * @brief 数字低通滤波器配置。
 * @note 枚举值直接写入 CONFIG 寄存器低 3 位；它同时决定陀螺内部输出率（8 kHz 或 1 kHz）。
 */
typedef enum mpu6050_dlpf_e {
  MPU6050_DLPF_256HZ = 0u,
  MPU6050_DLPF_188HZ,
  MPU6050_DLPF_98HZ,
  MPU6050_DLPF_42HZ,
  MPU6050_DLPF_20HZ,
  MPU6050_DLPF_10HZ,
  MPU6050_DLPF_5HZ,
} mpu6050_dlpf_t;

/**
 * @brief 陀螺仪量程配置。
 * @note 枚举值左移 3 位写入 GYRO_CONFIG；对应灵敏度见实现中的 LSB/(deg/s) 表。
 */
typedef enum mpu6050_gyro_range_e {
  MPU6050_GYRO_RANGE_250DPS = 0u,
  MPU6050_GYRO_RANGE_500DPS,
  MPU6050_GYRO_RANGE_1000DPS,
  MPU6050_GYRO_RANGE_2000DPS,
} mpu6050_gyro_range_t;

/**
 * @brief 加速度计量程配置。
 * @note 枚举值左移 3 位写入 ACCEL_CONFIG；对应灵敏度见实现中的 LSB/g 表。
 */
typedef enum mpu6050_accel_range_e {
  MPU6050_ACCEL_RANGE_2G = 0u,
  MPU6050_ACCEL_RANGE_4G,
  MPU6050_ACCEL_RANGE_8G,
  MPU6050_ACCEL_RANGE_16G,
} mpu6050_accel_range_t;

/**
 * @brief MPU6050 初始化配置。
 * @note 字段契约：
 *       i2c 为已由 CubeMX 初始化的逻辑实例，运行期不可切换；
 *       address 为 7 位地址，只允许 MPU6050_ADDRESS_AD0_LOW 或 HIGH；
 *       dlpf、gyro_range、accel_range 分别使用对应枚举值；
 *       sample_rate_hz 单位 Hz，合法范围 4..1000，实际周期取整到 ms 且最小 1 ms；
 *       calibration_samples 为上电静止标定样本数，范围 0..1000，0 表示跳过并令零偏为 0；
 *       i2c_timeout_ms 单位 ms 且必须大于 0；
 *       axis_source 每项范围 0..2 且三项互不重复；axis_sign 只能为 +1 或 -1。
 * @note 结构体内容在 Init 时整体复制；配置对象归调用方所有，Init 返回后可释放或复用。
 * @note axis_source/axis_sign 同时作用于加速度和陀螺仪，不做单独映射。
 * @note sample_rate_hz 与 DLPF 组合出的 SMPLRT_DIV 必须落在 8 位范围内，否则 Init 返回 ERROR。
 */
typedef struct mpu6050_config_s {
  platform_i2c_instance_t i2c;
  uint8_t address;
  mpu6050_dlpf_t dlpf;
  mpu6050_gyro_range_t gyro_range;
  mpu6050_accel_range_t accel_range;
  uint16_t sample_rate_hz;
  uint16_t calibration_samples;
  uint32_t i2c_timeout_ms;
  uint8_t axis_source[3];
  int8_t axis_sign[3];
} mpu6050_config_t;

/**
 * @brief 未经软件滤波的工程单位样本。
 * @note 字段契约：
 *       accel_mps2 为车体轴加速度，单位 m/s²，已做轴映射和量程换算；
 *       gyro_dps 为车体轴角速度，单位 deg/s，已做轴映射，READY 后已扣除静态零偏；
 *       temperature_c 为芯片温度，单位 °C，未做额外校准；
 *       time_ms 为采样发起时的单调毫秒时间戳，单位 ms。
 * @note 快照由驱动在临界区内整体复制，读取方持有副本，不共享指针。
 */
typedef struct mpu6050_raw_sample_s {
  float accel_mps2[3];
  float gyro_dps[3];
  float temperature_c;
  uint32_t time_ms;
} mpu6050_raw_sample_t;

/**
 * @brief MPU6050 状态机。
 * @note UNINITIALIZED 仅出现在 Init 探测期间；ERROR 为终态，process 不再重试。
 * @note CALIBRATE_PENDING/CALIBRATING 期间不发布新样本；READY 阶段按 sample_period_ms 轮询。
 */
typedef enum mpu6050_state_e {
  MPU6050_STATE_UNINITIALIZED = 0u,
  MPU6050_STATE_RESET_PENDING,
  MPU6050_STATE_RESET_WAIT,
  MPU6050_STATE_CALIBRATE_PENDING,
  MPU6050_STATE_CALIBRATING,
  MPU6050_STATE_READY,
  MPU6050_STATE_ERROR,
} mpu6050_state_t;

/**
 * @brief MPU6050 具体对象。
 * @note imu_base 必须是首成员，ops 回调据此还原具体对象。
 * @note config 是 Init 时的完整副本；i2c、address、灵敏度和周期由配置派生，运行期视为只读。
 * @note raw_sample/has_raw_sample 由传感器任务单写、任意任务经临界区读取。
 * @note gyro_bias_dps 只在标定完成时写入，READY 阶段只读。
 * @note 计数器为 volatile 但非原子；一致性读数应通过 base 状态快照获取。
 * @note 对象不允许复制，且必须静态分配；filter_h 是非 owning 指针，生命周期由调用方保证。
 */
typedef struct mpu6050_i2c_s {
  imu_base_t imu_base;
  platform_i2c_instance_t i2c;
  uint8_t address;
  mpu6050_config_t config;

  mpu6050_state_t state;
  bool is_started;
  uint32_t state_deadline_ms;
  uint32_t next_sample_ms;
  uint32_t sample_period_ms;
  uint8_t sample_rate_divider;

  float accel_lsb_per_g;
  float gyro_lsb_per_dps;
  float gyro_bias_dps[3];

  uint32_t calibration_count;
  float calibration_gyro_sum[3];
  float calibration_gyro_sum_sq[3];

  mpu6050_raw_sample_t raw_sample;
  volatile bool has_raw_sample;
  volatile bool last_request_failed;
  volatile uint32_t i2c_error_count;
  volatile uint32_t configuration_error_count;
  volatile uint32_t calibration_error_count;
  volatile uint32_t filter_reject_count;
} mpu6050_i2c_t;

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
);

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
);

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
);

/** @} */

#endif //LAYERED_EMBEDDED_C_MPU6050_I2C_H
