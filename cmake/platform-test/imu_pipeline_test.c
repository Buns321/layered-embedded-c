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

static void fake_i2c_reset(void) {
  memset(s_registers, 0, sizeof(s_registers));
  memset(s_burst, 0, sizeof(s_burst));
  s_registers[0x75u] = MPU6050_WHO_AM_I_VALUE;
  s_i2c_fail = false;
}

static void fake_i2c_set_accel_raw(int16_t x, int16_t y, int16_t z) {
  s_burst[0] = (uint8_t)((uint16_t)x >> 8u);
  s_burst[1] = (uint8_t)x;
  s_burst[2] = (uint8_t)((uint16_t)y >> 8u);
  s_burst[3] = (uint8_t)y;
  s_burst[4] = (uint8_t)((uint16_t)z >> 8u);
  s_burst[5] = (uint8_t)z;
}

static void fake_i2c_set_gyro_raw(int16_t x, int16_t y, int16_t z) {
  s_burst[8] = (uint8_t)((uint16_t)x >> 8u);
  s_burst[9] = (uint8_t)x;
  s_burst[10] = (uint8_t)((uint16_t)y >> 8u);
  s_burst[11] = (uint8_t)y;
  s_burst[12] = (uint8_t)((uint16_t)z >> 8u);
  s_burst[13] = (uint8_t)z;
}

static void fake_i2c_set_temperature_raw(int16_t value) {
  s_burst[6] = (uint8_t)((uint16_t)value >> 8u);
  s_burst[7] = (uint8_t)value;
}

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

void Sys_Critical_Enter(void) {
}

void Sys_Critical_Exit(void) {
}

typedef struct {
  imu_base_t base;
  bool started;
} test_imu_t;

static void test_imu_start(imu_base_t *imu_h) {
  test_imu_t *test_imu_h = (test_imu_t *)imu_h;
  test_imu_h->started = true;
}

static void test_imu_process(imu_base_t *imu_h, uint32_t now_ms) {
  (void)imu_h;
  (void)now_ms;
}

static void test_imu_get_status(imu_base_t *imu_h, imu_status_t *status) {
  (void)imu_h;
  *status = (imu_status_t){0};
}

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

static void test_attitude_filter_reset(filter_base_t *filter_h) {
  (void)filter_h;
}

static const filter_ops_t s_test_attitude_filter_ops = {
  .update = test_attitude_filter_update,
  .reset = test_attitude_filter_reset,
};

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
