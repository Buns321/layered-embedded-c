/**
 * @file imu_pipeline_test.c
 * @brief IMU 滤波管线与具体驱动测试。
 * @ingroup tests
 */

#include "filter_passthrough.h"
#include "imu_wht101_uart.h"
#include "mpu6050_i2c.h"
#include "system_critical.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do {                                                \
  if (!(condition)) {                                                        \
    (void)fprintf(                                                           \
      stderr,                                                                \
      "%s:%d: check failed: %s\n",                                           \
      __FILE__,                                                              \
      __LINE__,                                                              \
      #condition                                                             \
    );                                                                       \
    return 1;                                                                \
  }                                                                          \
} while (0)

#define CHECK_NEAR(actual, expected, tolerance) \
  CHECK(fabsf((actual) - (expected)) <= (tolerance))

#define FAKE_REG_COUNT 128u

static uint8_t s_registers[FAKE_REG_COUNT];
static uint8_t s_burst[14];
static bool s_i2c_fail;

/**
 * @brief 重置伪 I2C 后端状态。
 * @pre s_registers 和 s_burst 具有编译期固定容量，可安全清零。
 * @post 全部伪寄存器与 14 字节突发缓冲区清零，0x75 写入 MPU6050 WHO_AM_I，
 *       s_i2c_fail 恢复为 false。
 * @note 不分配动态内存，不访问真实 I2C；后续测试应显式设置所需原始值。
 */
static void fake_i2c_reset(void) {
  memset(s_registers, 0, sizeof(s_registers));
  memset(s_burst, 0, sizeof(s_burst));
  s_registers[0x75u] = MPU6050_WHO_AM_I_VALUE;
  s_i2c_fail = false;
}

/**
 * @brief 设置伪 I2C 后端返回的加速度原始值。
 * @param x X 轴有符号原始读数，单位为 MPU6050 加速度 LSB。
 * @param y Y 轴有符号原始读数，单位为 MPU6050 加速度 LSB。
 * @param z Z 轴有符号原始读数，单位为 MPU6050 加速度 LSB。
 * @pre s_burst 已按 MPU6050 突发布局预留字节 0..5。
 * @post 仅改写 s_burst[0..5]，每轴按大端顺序写入两个字节。
 * @note 不做量程换算，不修改温度或角速度字节。
 */
static void fake_i2c_set_accel_raw(int16_t x, int16_t y, int16_t z) {
  s_burst[0] = (uint8_t)((uint16_t)x >> 8u);
  s_burst[1] = (uint8_t)x;
  s_burst[2] = (uint8_t)((uint16_t)y >> 8u);
  s_burst[3] = (uint8_t)y;
  s_burst[4] = (uint8_t)((uint16_t)z >> 8u);
  s_burst[5] = (uint8_t)z;
}

/**
 * @brief 设置伪 I2C 后端返回的角速度原始值。
 * @param x X 轴有符号原始读数，单位为 MPU6050 陀螺 LSB。
 * @param y Y 轴有符号原始读数，单位为 MPU6050 陀螺 LSB。
 * @param z Z 轴有符号原始读数，单位为 MPU6050 陀螺 LSB。
 * @pre s_burst 已按 MPU6050 突发布局预留字节 8..13。
 * @post 仅改写 s_burst[8..13]，每轴按大端顺序写入两个字节。
 * @note 不做量程换算，不修改加速度或温度字节。
 */
static void fake_i2c_set_gyro_raw(int16_t x, int16_t y, int16_t z) {
  s_burst[8] = (uint8_t)((uint16_t)x >> 8u);
  s_burst[9] = (uint8_t)x;
  s_burst[10] = (uint8_t)((uint16_t)y >> 8u);
  s_burst[11] = (uint8_t)y;
  s_burst[12] = (uint8_t)((uint16_t)z >> 8u);
  s_burst[13] = (uint8_t)z;
}

/**
 * @brief 设置伪 I2C 后端返回的温度原始值。
 * @param value 有符号温度原始读数，单位为 MPU6050 温度 LSB。
 * @pre s_burst 已按 MPU6050 突发布局预留字节 6..7。
 * @post 仅改写 s_burst[6..7]，按大端顺序写入两个字节。
 * @note 驱动负责按数据手册公式换算为摄氏度；本函数不进行换算。
 */
static void fake_i2c_set_temperature_raw(int16_t value) {
  s_burst[6] = (uint8_t)((uint16_t)value >> 8u);
  s_burst[7] = (uint8_t)value;
}

/**
 * @brief 模拟 I2C 存储器写，把数据复制到伪寄存器数组。
 * @param i2c I2C 实例；本替身只接受 PLATFORM_I2C_1。
 * @param address 7 位器件地址；本替身只接受 MPU6050_ADDRESS_AD0_LOW。
 * @param mem_address 起始伪寄存器地址；加上写入长度后不得越过 s_registers 容量。
 * @param mem_address_size 存储地址宽度（字节）；本替身只接受 1。
 * @param data 发送缓冲区；必须非 NULL，调用期间只读且至少包含 size 字节。
 * @param size 写入字节数；必须大于 0，且写范围不得越过伪寄存器数组。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 全部数据已复制到 s_registers。
 * @retval PLATFORM_STATUS_ERROR s_i2c_fail 为 true、实例/地址/宽度/缓冲区/长度不合法，
 *         或写入范围越过 s_registers；越界前已复制的前缀不回滚。
 * @pre 调用前可通过 fake_i2c_reset 清零伪寄存器；data 生命周期覆盖调用。
 * @post 成功时 s_registers 对应区间被覆盖；失败时不访问真实 I2C，但可能留下部分写入。
 * @warning 这是测试替身，不产生总线时序，不映射 BUSY、TIMEOUT 或 INVALID_ARGUMENT。
 */
platform_status_t Platform_I2C_MemWrite(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  const uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)timeout;

  if (
    s_i2c_fail ||
    i2c != PLATFORM_I2C_1 ||
    address != MPU6050_ADDRESS_AD0_LOW ||
    mem_address_size != 1u ||
    data == NULL ||
    size == 0u ||
    mem_address >= FAKE_REG_COUNT
  ) {
    return PLATFORM_STATUS_ERROR;
  }

  for (uint16_t i = 0u; i < size; i++) {
    uint16_t reg = (uint16_t)(mem_address + i);
    if (reg >= FAKE_REG_COUNT) {
      return PLATFORM_STATUS_ERROR;
    }
    s_registers[reg] = data[i];
  }

  return PLATFORM_STATUS_OK;
}

/**
 * @brief 模拟 I2C 存储器读，从伪寄存器或突发缓冲区复制数据。
 * @param i2c I2C 实例；本替身只接受 PLATFORM_I2C_1。
 * @param address 7 位器件地址；本替身只接受 MPU6050_ADDRESS_AD0_LOW。
 * @param mem_address 起始伪寄存器地址；读取范围不得越过 s_registers 容量。
 * @param mem_address_size 存储地址宽度（字节）；本替身只接受 1。
 * @param data 接收缓冲区；必须非 NULL，调用期间可写且容量至少为 size 字节。
 * @param size 读取字节数；必须大于 0。
 * @param timeout 超时时间，单位 ms；本替身忽略该参数且不等待。
 * @retval PLATFORM_STATUS_OK 已从 0x3B 的 14 字节突发缓冲区或 s_registers 复制数据。
 * @retval PLATFORM_STATUS_ERROR s_i2c_fail 为 true、实例/地址/宽度/缓冲区/长度不合法，
 *         或读取范围越过 s_registers；越界前已复制的前缀不回滚。
 * @pre 测试可通过 fake_i2c_reset 和 fake_i2c_set_*_raw 准备伪数据；data 生命周期覆盖调用。
 * @post data 前 size 字节被覆盖；失败时不访问真实 I2C，但可能留下部分复制结果。
 * @warning 这是测试替身，不产生重复起始或停止条件，也不映射 BUSY、TIMEOUT 或 INVALID_ARGUMENT。
 */
platform_status_t Platform_I2C_MemRead(
  platform_i2c_instance_t i2c,
  uint16_t address,
  uint16_t mem_address,
  uint8_t mem_address_size,
  uint8_t *data,
  uint16_t size,
  uint32_t timeout
) {
  (void)timeout;

  if (
    s_i2c_fail ||
    i2c != PLATFORM_I2C_1 ||
    address != MPU6050_ADDRESS_AD0_LOW ||
    mem_address_size != 1u ||
    data == NULL ||
    size == 0u
  ) {
    return PLATFORM_STATUS_ERROR;
  }

  if (mem_address == 0x3Bu && size == sizeof(s_burst)) {
    memcpy(data, s_burst, sizeof(s_burst));
    return PLATFORM_STATUS_OK;
  }

  for (uint16_t i = 0u; i < size; i++) {
    uint16_t reg = (uint16_t)(mem_address + i);
    if (reg >= FAKE_REG_COUNT) {
      return PLATFORM_STATUS_ERROR;
    }
    data[i] = s_registers[reg];
  }

  return PLATFORM_STATUS_OK;
}

/**
 * @brief IMU 管线测试使用的空临界区进入桩。
 * @pre 必须与 Sys_Critical_Exit 配对调用；本测试不在 ISR 中调用。
 * @post 函数体为空，不保存 PRIMASK、不关闭中断、不增加嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让 IMU 基类和具体驱动完成链接。
 * @note 本测试后续不依赖真实临界区；生产 System 实现才有中断/调度保护语义。
 */
void Sys_Critical_Enter(void) {
}

/**
 * @brief IMU 管线测试使用的空临界区退出桩。
 * @pre 之前已调用本文件的 Sys_Critical_Enter 桩；本函数不验证配对关系。
 * @post 函数体为空，不恢复 PRIMASK、不开启中断、不修改嵌套计数。
 * @warning 该桩不提供互斥或中断保护，只用于让 IMU 基类和具体驱动完成链接。
 * @note 必须与 Sys_Critical_Enter 成对调用；生产 System 实现才有恢复语义。
 */
void Sys_Critical_Exit(void) {
}

typedef struct {
  imu_base_t base;
  bool started;
} test_imu_t;

/**
 * @brief 验证 IMU 启动流程。
 * @param imu_h 传给具体 IMU ops 的基对象指针；实际必须指向 test_imu_t。
 * @pre imu_h 非空，且其布局以 imu_base_t 开头，后接 bool started。
 * @post test_imu_t.started 置为 true；不初始化滤波器或访问硬件。
 * @note 该替身让 Dev_IMU_Base_Start 的可观察启动标志可被测试断言。
 * @warning 传入其他对象类型会写越界并使测试进程崩溃。
 */
static void test_imu_start(imu_base_t *imu_h) {
  test_imu_t *test_imu_h = (test_imu_t *)imu_h;
  test_imu_h->started = true;
}

/**
 * @brief 验证 IMU 周期处理。
 * @param imu_h IMU 基对象；本测试替身不访问该参数。
 * @param now_ms 当前单调毫秒时间戳；本测试替身不访问该参数。
 * @pre 无；允许任意指针和任意时间戳，因为函数只消费参数。
 * @post 不修改 IMU、时间戳或任何全局状态。
 * @note 用于让不依赖周期采集的通用管线测试通过 ops 调用。
 */
static void test_imu_process(imu_base_t *imu_h, uint32_t now_ms) {
  (void)imu_h;
  (void)now_ms;
}

/**
 * @brief 验证 IMU 状态快照。
 * @param imu_h IMU 基对象；本测试替身不访问该参数。
 * @param status 接收状态快照的可写地址；必须非空。
 * @pre status 指向至少一个 imu_status_t 的存储，生命周期覆盖本次调用。
 * @post *status 被整体清零，不反映真实启动、忙、样本数或错误数据。
 * @warning 传 NULL 会直接解引用并终止进程。
 * @note 公共 Dev_IMU_Base_GetStatus 会覆盖本替身输出，因此本函数只满足 ops 契约。
 */
static void test_imu_get_status(imu_base_t *imu_h, imu_status_t *status) {
  (void)imu_h;
  *status = (imu_status_t){0};
}

/**
 * @brief 验证 IMU 航向置零。
 * @param imu_h 已初始化的 IMU 基对象；必须非空。
 * @pre 对象已建立滤波器和样本快照；调用时 yaw 通道可有效也可无效。
 * @post 转调 Dev_IMU_Base_ZeroYaw；若当前样本含 yaw，则更新偏移并把发布 yaw 置零，
 *       不修改 raw_sample 或滤波器内部状态。
 * @warning Dev_IMU_Base_ZeroYaw 内的空指针断言失败会终止进程。
 */
static void test_imu_set_zero(imu_base_t *imu_h) {
  Dev_IMU_Base_ZeroYaw(imu_h);
}

static const imu_ops_t s_test_imu_ops = {
  .start = test_imu_start,
  .process = test_imu_process,
  .get_status = test_imu_get_status,
  .set_zero = test_imu_set_zero,
};

typedef struct {
  uart_base_t base;
  uint32_t receive_count;
} test_uart_t;

/**
 * @brief 验证 UART 中断接收。
 * @param uart_h UART 基对象；实际必须指向 test_uart_t。
 * @param data 接收缓冲区；本测试桩不访问或保存该指针。
 * @param size 请求接收字节数；本测试桩不访问该参数。
 * @retval DEV_UART_OK 始终返回，表示测试桩已接受接收请求。
 * @pre uart_h 非空且以 uart_base_t 开头，后接 receive_count。
 * @post test_uart_t.receive_count 加一；不启动真实 UART 中断。
 * @warning 传入其他对象类型会写越界并使测试进程崩溃。
 */
static dev_uart_status_t test_uart_receive_it(
  uart_base_t *uart_h,
  uint8_t *data,
  uint16_t size
) {
  test_uart_t *test_uart_h = (test_uart_t *)uart_h;
  (void)data;
  (void)size;
  test_uart_h->receive_count++;
  return DEV_UART_OK;
}

static const uart_ops_t s_test_uart_ops = {
  .receive_it = test_uart_receive_it,
};

typedef struct {
  filter_base_t base;
} test_attitude_filter_t;

/**
 * @brief 验证姿态滤波器更新结果。
 * @param filter_h 滤波器基对象；本测试替身不访问该参数。
 * @param input 输入样本；本测试替身不读取该参数。
 * @param output 待修改的滤波器输出样本；必须非空且可写。
 * @pre Dev_IMU_Base 已先把输入通道复制到 output，允许本函数覆盖指定通道。
 * @post 清除 output.valid_mask 中的 GYRO_X，置位 YAW，并把 yaw 值写为 45.0 度。
 * @warning 传 NULL 输出会直接解引用并终止进程。
 * @note 该行为用于验证 base 会合并新增姿态通道并尊重滤波器清除的通道。
 */
static void test_attitude_filter_update(
  filter_base_t *filter_h,
  const filter_sample_t *input,
  filter_sample_t *output
) {
  (void)filter_h;
  (void)input;

  output->valid_mask &= ~IMU_CHANNEL_GYRO_X;
  output->valid_mask |= IMU_CHANNEL_YAW;
  output->value[IMU_CHANNEL_INDEX_YAW] = 45.0f;
}

/**
 * @brief 验证姿态滤波器重置清空状态。
 * @param filter_h 滤波器基对象；本测试替身不访问该参数。
 * @pre 无；允许任意指针或无内部状态的测试滤波器。
 * @post 不修改滤波器或样本状态，为空操作。
 * @note 用于满足 filter_ops_t.reset 契约；不验证真实滤波器复位效果。
 */
static void test_attitude_filter_reset(filter_base_t *filter_h) {
  (void)filter_h;
}

static const filter_ops_t s_test_attitude_filter_ops = {
  .update = test_attitude_filter_update,
  .reset = test_attitude_filter_reset,
};

/**
 * @brief 验证不完整 IMU 帧处理。
 * @retval 0 三个单通道样本被合并且各自更新时间保留，yaw 置零与后续相对值正确。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 测试 IMU ops 和 passthrough 滤波器已链接，对象可栈上初始化。
 * @post 最终样本序号为 4；yaw、GYRO_Z、ACCEL_X 通道有效且值分别为 15 度、
 *       12.5 dps、1.25 m/s^2；status 显示已启动、有时间戳且样本数为 4。
 * @note 覆盖分通道提交、时间戳保留、ZeroYaw 偏移和后续相对 yaw。
 * @warning CHECK 失败返回 1；底层断言触发时进程会终止。
 */
static int test_partial_imu_frames(void) {
  test_imu_t imu = {0};
  filter_passthrough_t passthrough;
  Filter_Passthrough_Init(&passthrough, "imu-test-passthrough");

  Dev_IMU_Base_Init(
    &imu.base,
    "imu-test",
    &s_test_imu_ops,
    &passthrough.filter_base
  );
  Dev_IMU_Base_Start(&imu.base);
  CHECK(imu.started);

  filter_sample_t yaw = {
    .valid_mask = IMU_CHANNEL_YAW,
    .time_ms = 100u,
  };
  yaw.value[IMU_CHANNEL_INDEX_YAW] = 30.0f;
  CHECK(Dev_IMU_Base_SubmitSample(&imu.base, &yaw));

  filter_sample_t gyro = {
    .valid_mask = IMU_CHANNEL_GYRO_Z,
    .time_ms = 110u,
  };
  gyro.value[IMU_CHANNEL_INDEX_GYRO_Z] = 12.5f;
  CHECK(Dev_IMU_Base_SubmitSample(&imu.base, &gyro));

  filter_sample_t accel = {
    .valid_mask = IMU_CHANNEL_ACCEL_X,
    .time_ms = 120u,
  };
  accel.value[IMU_CHANNEL_INDEX_ACCEL_X] = 1.25f;
  CHECK(Dev_IMU_Base_SubmitSample(&imu.base, &accel));

  imu_sample_t sample = {0};
  CHECK(Dev_IMU_Base_GetSample(&imu.base, &sample));
  CHECK(sample.sequence == 3u);
  CHECK((sample.valid_mask & IMU_CHANNEL_YAW) != 0u);
  CHECK((sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0u);
  CHECK((sample.valid_mask & IMU_CHANNEL_ACCEL_X) != 0u);
  CHECK_NEAR(sample.yaw_deg, 30.0f, 0.001f);
  CHECK_NEAR(sample.gyro_dps[2], 12.5f, 0.001f);
  CHECK_NEAR(sample.accel_mps2[0], 1.25f, 0.001f);
  CHECK(sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW] == 100u);
  CHECK(sample.channel_update_ms[IMU_CHANNEL_INDEX_GYRO_Z] == 110u);
  CHECK(sample.channel_update_ms[IMU_CHANNEL_INDEX_ACCEL_X] == 120u);

  Dev_IMU_Base_ZeroYaw(&imu.base);
  CHECK(Dev_IMU_Base_GetSample(&imu.base, &sample));
  CHECK_NEAR(sample.yaw_deg, 0.0f, 0.001f);

  yaw.time_ms = 130u;
  yaw.value[IMU_CHANNEL_INDEX_YAW] = 45.0f;
  CHECK(Dev_IMU_Base_SubmitSample(&imu.base, &yaw));
  CHECK(Dev_IMU_Base_GetSample(&imu.base, &sample));
  CHECK_NEAR(sample.yaw_deg, 15.0f, 0.001f);

  imu_status_t status = {0};
  Dev_IMU_Base_GetStatus(&imu.base, &status);
  CHECK(status.is_started);
  CHECK(status.has_timestamp);
  CHECK(status.sample_count == 4u);
  CHECK(status.channel_update_ms[IMU_CHANNEL_INDEX_YAW] == 130u);
  return 0;
}

/**
 * @brief 验证滤波器姿态输出与通道清除。
 * @retval 0 滤波器新增 YAW、清除 GYRO_X，且保留未清除的 GYRO_Z。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 姿态滤波测试 ops 已链接，测试 IMU 与滤波器可在栈上构造。
 * @post 发布样本的 GYRO_X 位被清除，GYRO_Z 仍有效，YAW 有效且值为 45 度。
 * @note 验证 base 以滤波器输出而非原始输入作为最终通道掩码。
 * @warning CHECK 失败返回 1；底层断言触发时进程会终止。
 */
static int test_filter_adds_attitude_and_clears_channel(void) {
  test_imu_t imu = {0};
  test_attitude_filter_t filter = {0};
  Filter_Base_Init(
    &filter.base,
    "imu-test-attitude-filter",
    &s_test_attitude_filter_ops
  );

  Dev_IMU_Base_Init(
    &imu.base,
    "imu-filter-test",
    &s_test_imu_ops,
    &filter.base
  );
  Dev_IMU_Base_Start(&imu.base);

  filter_sample_t frame = {
    .valid_mask = IMU_CHANNEL_GYRO_X | IMU_CHANNEL_GYRO_Z,
    .time_ms = 300u,
  };
  frame.value[IMU_CHANNEL_INDEX_GYRO_X] = 1.0f;
  frame.value[IMU_CHANNEL_INDEX_GYRO_Z] = 2.0f;
  CHECK(Dev_IMU_Base_SubmitSample(&imu.base, &frame));

  imu_sample_t sample = {0};
  CHECK(Dev_IMU_Base_GetSample(&imu.base, &sample));
  CHECK((sample.valid_mask & IMU_CHANNEL_GYRO_X) == 0u);
  CHECK((sample.valid_mask & IMU_CHANNEL_GYRO_Z) != 0u);
  CHECK((sample.valid_mask & IMU_CHANNEL_YAW) != 0u);
  CHECK_NEAR(sample.yaw_deg, 45.0f, 0.001f);
  return 0;
}

/**
 * @brief 向 WHT101 接收路径注入完整帧。
 * @param imu 已初始化并已启动的 WHT101 UART IMU 对象。
 * @param type WHT101 帧类型字节，测试使用 GYRO 或 ANGLE。
 * @param value 各类型对应的有符号 16 位原始值，未做量纲换算。
 * @param now_ms 该帧的单调毫秒时间戳；所有字节共享此时间。
 * @pre imu 非空且接收状态机已初始化；now_ms 由调用方保持单调递增。
 * @post 构造带帧头、类型、小端数值和累加校验和的完整帧，逐字节交给 FeedByte；
 *       解析成功后可能更新当前样本和通道更新时间。
 * @warning Dev_IMU_WHT101_UART_FeedByte 对空对象使用 assert，失败会终止进程。
 * @note 不经过真实 UART ISR；该辅助函数模拟任务侧解析输入。
 */
static void feed_wht101_frame(
  imu_wht101_uart_t *imu,
  uint8_t type,
  int16_t value,
  uint32_t now_ms
) {
  uint8_t frame[IMU_WHT101_FRAME_LEN] = {0};
  frame[0] = IMU_WHT101_FRAME_HEADER;
  frame[1] = type;
  frame[6] = (uint8_t)((uint16_t)value & 0xFFu);
  frame[7] = (uint8_t)((uint16_t)value >> 8u);

  uint8_t checksum = 0u;
  for (uint8_t i = 0u; i < IMU_WHT101_FRAME_LEN - 1u; i++) {
    checksum = (uint8_t)(checksum + frame[i]);
  }
  frame[IMU_WHT101_FRAME_LEN - 1u] = checksum;

  for (uint8_t i = 0u; i < IMU_WHT101_FRAME_LEN; i++) {
    Dev_IMU_WHT101_UART_FeedByte(imu, frame[i], now_ms);
  }
}

/**
 * @brief 验证 WHT101 透传管线。
 * @retval 0 启动挂接一次接收，GYRO/ANGLE 两帧发布预期 yaw、角速度和时间戳。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre WHT101 驱动、UART 测试替身和 passthrough 滤波器已链接。
 * @post 最终有效通道恰为 YAW 与 GYRO_Z；yaw 为 90 度、z 轴角速度为 10 dps；
 *       YAW 更新时间 210 ms，GYRO_Z 更新时间 200 ms。
 * @note 覆盖小端协议解析、校验和、类型分派及工程单位换算。
 * @warning CHECK 失败返回 1；底层断言触发时进程会终止。
 */
static int test_wht101_passthrough_pipeline(void) {
  test_uart_t usart = {0};
  Dev_UART_Base_Init(&usart.base, "wht-test-usart", &s_test_uart_ops);

  filter_passthrough_t passthrough;
  Filter_Passthrough_Init(&passthrough, "wht-test-passthrough");

  imu_wht101_uart_t imu = {0};
  Dev_IMU_WHT101_UART_Init(
    &imu,
    "wht-test",
    &usart.base,
    &passthrough.filter_base
  );

  Dev_IMU_Base_Start(&imu.imu_base);
  CHECK(usart.receive_count == 1u);

  feed_wht101_frame(
    &imu,
    IMU_WHT101_TYPE_GYRO,
    164,
    200u
  );
  feed_wht101_frame(
    &imu,
    IMU_WHT101_TYPE_ANGLE,
    16384,
    210u
  );

  imu_sample_t sample = {0};
  CHECK(Dev_IMU_Base_GetSample(&imu.imu_base, &sample));
  CHECK(sample.valid_mask == (
    IMU_CHANNEL_YAW |
    IMU_CHANNEL_GYRO_Z
  ));
  CHECK_NEAR(sample.yaw_deg, 90.0f, 0.01f);
  CHECK_NEAR(sample.gyro_dps[2], 10.0f, 0.02f);
  CHECK(sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW] == 210u);
  CHECK(sample.channel_update_ms[IMU_CHANNEL_INDEX_GYRO_Z] == 200u);
  return 0;
}

/**
 * @brief 构造有效的 MPU6050 测试配置。
 * @param config 接收完整配置的可写对象；必须非空。
 * @param axis_source 三个目标轴的原始轴索引；索引 0/1/2 对应 X/Y/Z。
 * @param axis_sign 三个轴的符号修正，取 +1 或 -1；本函数不校验范围。
 * @pre config 生命周期覆盖调用；axis_source 和 axis_sign 各可读 3 个元素。
 * @post config 被整体赋值为 I2C1、AD0 低地址、42 Hz DLPF、500 dps 陀螺、
 *       4 g 加速度、100 Hz 采样、2 次校准和 20 ms 超时，并复制轴映射数组。
 * @warning 空指针或不足 3 元素的数组会越界访问；无断言保护。
 * @note 该配置供 MPU6050 初始化与采集测试复用；所有权仍归调用方。
 */
static void init_mpu_config(
  mpu6050_config_t *config,
  const uint8_t axis_source[3],
  const int8_t axis_sign[3]
) {
  *config = (mpu6050_config_t){
    .i2c = PLATFORM_I2C_1,
    .address = MPU6050_ADDRESS_AD0_LOW,
    .dlpf = MPU6050_DLPF_42HZ,
    .gyro_range = MPU6050_GYRO_RANGE_500DPS,
    .accel_range = MPU6050_ACCEL_RANGE_4G,
    .sample_rate_hz = 100u,
    .calibration_samples = 2u,
    .i2c_timeout_ms = 20u,
    .axis_source = {
      axis_source[0],
      axis_source[1],
      axis_source[2],
    },
    .axis_sign = {
      axis_sign[0],
      axis_sign[1],
      axis_sign[2],
    },
  };
}

/**
 * @brief 验证 MPU6050 采样与校准流程。
 * @retval 0 初始化、配置寄存器、校准后发布六轴样本、原始温度和状态均符合预期。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 伪 I2C 寄存器初始化为已知值，passthrough 与 MPU6050 驱动已链接。
 * @post 发布 z 轴加速度约 9.80665 m/s^2、z 轴角速度约 0 dps；
 *       原始温度约 46.53 摄氏度且时间戳 112 ms；状态已启动、不忙、零错误、样本数 1；
 *       伪寄存器 0x19=9、DLPF 与两轴量程位按配置写入。
 * @note 覆盖重复时间戳抑制、2 样本陀螺校准和突发读取后的单位换算。
 * @warning CHECK 失败返回 1；驱动或基础层断言触发时进程会终止。
 */
static int test_mpu6050_acquisition(void) {
  fake_i2c_reset();

  filter_passthrough_t passthrough;
  Filter_Passthrough_Init(&passthrough, "mpu-test-passthrough");

  mpu6050_i2c_t mpu = {0};
  const uint8_t axis_source[3] = {0u, 1u, 2u};
  const int8_t axis_sign[3] = {1, 1, 1};
  mpu6050_config_t config;
  init_mpu_config(&config, axis_source, axis_sign);

  CHECK(
    Dev_MPU6050_I2C_Init(
      &mpu,
      "mpu-test",
      &config,
      &passthrough.filter_base
    ) == PLATFORM_STATUS_OK
  );

  fake_i2c_set_accel_raw(0, 0, 8192);
  fake_i2c_set_gyro_raw(0, 0, 655);
  fake_i2c_set_temperature_raw(3400);

  Dev_IMU_Base_Start(&mpu.imu_base);
  Dev_IMU_Base_Process(&mpu.imu_base, 0u);
  Dev_IMU_Base_Process(&mpu.imu_base, 101u);
  Dev_IMU_Base_Process(&mpu.imu_base, 101u);
  Dev_IMU_Base_Process(&mpu.imu_base, 101u);
  Dev_IMU_Base_Process(&mpu.imu_base, 111u);
  Dev_IMU_Base_Process(&mpu.imu_base, 112u);

  imu_sample_t sample = {0};
  CHECK(Dev_IMU_Base_GetSample(&mpu.imu_base, &sample));
  CHECK(sample.valid_mask == (
    IMU_CHANNEL_ACCEL_X |
    IMU_CHANNEL_ACCEL_Y |
    IMU_CHANNEL_ACCEL_Z |
    IMU_CHANNEL_GYRO_X |
    IMU_CHANNEL_GYRO_Y |
    IMU_CHANNEL_GYRO_Z
  ));
  CHECK_NEAR(sample.accel_mps2[2], 9.80665f, 0.01f);
  CHECK_NEAR(sample.gyro_dps[2], 0.0f, 0.01f);

  mpu6050_raw_sample_t raw = {0};
  CHECK(Dev_MPU6050_I2C_GetRawSample(&mpu, &raw));
  CHECK_NEAR(raw.temperature_c, 46.53f, 0.01f);
  CHECK(raw.time_ms == 112u);

  imu_status_t status = {0};
  Dev_IMU_Base_GetStatus(&mpu.imu_base, &status);
  CHECK(status.is_started);
  CHECK(!status.is_busy);
  CHECK(status.error_count == 0u);
  CHECK(status.sample_count == 1u);
  CHECK(
    s_registers[0x19u] == 9u &&
    (s_registers[0x1Au] & 0x07u) == MPU6050_DLPF_42HZ &&
    (s_registers[0x1Bu] & 0x18u) == (1u << 3u) &&
    (s_registers[0x1Cu] & 0x18u) == (1u << 3u)
  );
  return 0;
}

/**
 * @brief 验证 MPU6050 轴映射和探测失败。
 * @retval 0 交换轴并取反后样本值正确，错误 WHO_AM_I 使初始化返回 ERROR。
 * @retval 1 任一 CHECK 失败；失败条件及源码位置写入 stderr。
 * @pre 伪 I2C 可分别返回正常和错误的 WHO_AM_I，passthrough 滤波器已链接。
 * @post 第一组样本得到 x 轴 -4.903325 m/s^2、y 轴 9.80665 m/s^2，
 *       陀螺 x/y 分别约 -2 和 10 dps；第二组对象初始化返回 PLATFORM_STATUS_ERROR。
 * @note 校准样本设为 0，以直接验证轴映射和符号；探测失败路径不发布样本。
 * @warning CHECK 失败返回 1；驱动或基础层断言触发时进程会终止。
 */
static int test_mpu6050_axis_mapping_and_probe_failure(void) {
  fake_i2c_reset();

  filter_passthrough_t passthrough;
  Filter_Passthrough_Init(&passthrough, "mpu-axis-passthrough");

  mpu6050_i2c_t mpu = {0};
  const uint8_t axis_source[3] = {1u, 0u, 2u};
  const int8_t axis_sign[3] = {-1, 1, 1};
  mpu6050_config_t config;
  init_mpu_config(&config, axis_source, axis_sign);
  config.calibration_samples = 0u;

  CHECK(
    Dev_MPU6050_I2C_Init(
      &mpu,
      "mpu-axis",
      &config,
      &passthrough.filter_base
    ) == PLATFORM_STATUS_OK
  );

  fake_i2c_set_accel_raw(8192, 4096, 0);
  fake_i2c_set_gyro_raw(655, 131, 0);

  Dev_IMU_Base_Start(&mpu.imu_base);
  Dev_IMU_Base_Process(&mpu.imu_base, 0u);
  Dev_IMU_Base_Process(&mpu.imu_base, 101u);
  Dev_IMU_Base_Process(&mpu.imu_base, 102u);
  Dev_IMU_Base_Process(&mpu.imu_base, 103u);

  imu_sample_t sample = {0};
  CHECK(Dev_IMU_Base_GetSample(&mpu.imu_base, &sample));
  CHECK_NEAR(sample.accel_mps2[0], -4.903325f, 0.01f);
  CHECK_NEAR(sample.accel_mps2[1], 9.80665f, 0.01f);
  CHECK_NEAR(sample.gyro_dps[0], -2.0f, 0.01f);
  CHECK_NEAR(sample.gyro_dps[1], 10.0f, 0.01f);

  fake_i2c_reset();
  s_registers[0x75u] = 0x00u;
  mpu6050_i2c_t invalid_mpu = {0};
  CHECK(
    Dev_MPU6050_I2C_Init(
      &invalid_mpu,
      "mpu-invalid",
      &config,
      &passthrough.filter_base
    ) == PLATFORM_STATUS_ERROR
  );
  return 0;
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 五个 IMU 管线用例全部通过，并向 stdout 输出成功信息。
 * @retval 1 首个失败用例返回非零；后续用例不再执行。
 * @pre IMU 基础层、passthrough 滤波、WHT101、MPU6050 和伪 I2C 已链接。
 * @post 成功时标准输出包含固定通过信息；无动态内存或真实硬件资源。
 * @warning 底层断言失败可能直接终止进程，而不是返回 1。
 */
int main(void) {
  if (test_partial_imu_frames() != 0) {
    return 1;
  }
  if (test_filter_adds_attitude_and_clears_channel() != 0) {
    return 1;
  }
  if (test_wht101_passthrough_pipeline() != 0) {
    return 1;
  }
  if (test_mpu6050_acquisition() != 0) {
    return 1;
  }
  if (test_mpu6050_axis_mapping_and_probe_failure() != 0) {
    return 1;
  }

  (void)puts("IMU pipeline tests passed");
  return 0;
}
