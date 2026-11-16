/**
 * @file app_car_dependencies_contract_test.c
 * @brief F407 小车 App 依赖校验和初始化契约测试。
 * @ingroup tests
 */

#include "app_car.h"
#include "app_car_internal.h"
#include "system_log.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

volatile uint32_t g_sys_log_fast_mask;

static chassis_base_t s_chassis;
static motor_base_t s_motor[APP_CAR_MOTOR_CAPACITY];
static imu_base_t s_imu;
static joycon_base_t s_input;
static telemetry_base_t s_telemetry;

/**
 * @brief 测试日志写入桩。
 * @param level 日志等级。
 * @param tag 日志标签。
 * @param format 格式字符串。
 * @param ... 格式参数。
 * @note 本测试关闭快速日志掩码，正常路径不会调用本函数。
 */
void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
) {
  (void)level;
  (void)tag;
  (void)format;
}

/**
 * @brief 测试临界区进入桩。
 * @post 无状态变化。
 */
void Sys_Critical_Enter(void) {
}

/**
 * @brief 测试临界区退出桩。
 * @post 无状态变化。
 */
void Sys_Critical_Exit(void) {
}

/**
 * @brief 验证 App 初始化依赖契约。
 * @return 0 表示通过。
 * @post App 拒绝空依赖和不完整依赖，并接受完整静态依赖。
 */
int main(void) {
  app_car_dependencies_t dependencies = {
    .chassis_h = &s_chassis,
    .imu_h = &s_imu,
    .input_h = &s_input,
    .telemetry_h = &s_telemetry,
    .motor_count = APP_CAR_MOTOR_CAPACITY,
  };
  for (uint8_t wheel = 0U; wheel < APP_CAR_MOTOR_CAPACITY; wheel++) {
    dependencies.motor_h[wheel] = &s_motor[wheel];
  }

  assert(!App_Car_Init(NULL));

  dependencies.chassis_h = NULL;
  assert(!App_Car_Init(&dependencies));
  dependencies.chassis_h = &s_chassis;

  dependencies.motor_h[2] = NULL;
  assert(!App_Car_Init(&dependencies));
  dependencies.motor_h[2] = &s_motor[2];

  dependencies.motor_count = 0U;
  assert(!App_Car_Init(&dependencies));
  dependencies.motor_count = APP_CAR_MOTOR_CAPACITY;

  assert(App_Car_Init(&dependencies));
  assert(App_Car_Init(&dependencies));

  uint32_t mask = app_car_build_telemetry_mask(2U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_TARGET_A)) != 0U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_TARGET_B)) != 0U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_TARGET_C)) == 0U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_TARGET_D)) == 0U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_CURRENT_C)) == 0U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_CURRENT_D)) == 0U);
  assert((mask & (1UL << APP_CAR_TELEMETRY_YAW)) != 0U);
  assert(app_car_build_telemetry_mask(APP_CAR_MOTOR_CAPACITY + 1U) == 0U);
  return 0;
}
