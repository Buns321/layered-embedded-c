/**
 * @file chassis_omni_contract_test.c
 * @brief Omni 底盘车体系 twist 与失效停车契约测试。
 * @ingroup tests
 */

#include "chassis_omni.h"

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct fake_policy_s {
  policy_omni_base_t base;
  uint32_t solve_count;
  float linear_speed_mps;
  float direction_rad;
  float yaw_rad;
  float angular_speed_radps;
} fake_policy_t;

static imu_sample_t s_imu_sample;
static bool s_imu_available;
static uint32_t s_target_update_count;
static uint32_t s_brake_count;
static uint32_t s_disable_count;

/**
 * @brief 记录 Policy 输入并写回固定四轮目标。
 * @param policy_h Policy 基类视图；必须指向 fake_policy_t 首成员。
 * @param v 世界系平动速度，单位 m/s。
 * @param dir 世界系方向，单位 rad。
 * @param yaw 当前 yaw，单位 rad。
 * @param omega 自转角速度，单位 rad/s。
 * @param out 四轮目标输出，不得为 NULL。
 * @post 输入被记录，四轮输出依次为 1、2、3、4 m/s。
 * @note 测试替身不执行真实运动学解算。
 */
static void fake_policy_solve(
  policy_omni_base_t *policy_h,
  float v,
  float dir,
  float yaw,
  float omega,
  float out[4]
) {
  fake_policy_t *policy = (fake_policy_t *)policy_h;
  policy->solve_count++;
  policy->linear_speed_mps = v;
  policy->direction_rad = dir;
  policy->yaw_rad = yaw;
  policy->angular_speed_radps = omega;
  for (uint8_t wheel = 0u; wheel < CHASSIS_OMNI_WHEEL_COUNT; wheel++) {
    out[wheel] = (float)(wheel + 1u);
  }
}

static const policy_omni_ops_t s_fake_policy_ops = {
  .solve = fake_policy_solve,
};

/**
 * @brief 复制测试 IMU 快照。
 * @param imu_h IMU 基类视图；测试替身不访问该参数。
 * @param sample 输出快照，不得为 NULL。
 * @retval true 测试快照可用。
 * @retval false 测试快照不可用。
 * @post output 被完整覆盖。
 * @note 该桩替代真实 IMU 管线。
 */
bool Dev_IMU_Base_GetSample(imu_base_t *imu_h, imu_sample_t *sample) {
  (void)imu_h;
  assert(sample);
  *sample = s_imu_sample;
  return s_imu_available;
}

/**
 * @brief 记录电机目标写入。
 * @param motor_h 电机基类视图，不得为 NULL。
 * @param target_speed_mps 目标速度，单位 m/s。
 * @post 目标写入电机对象并递增调用计数。
 * @note 测试替身不运行控制律。
 */
void Dev_Motor_Base_SetTargetSpeed(
  motor_base_t *motor_h,
  float target_speed_mps
) {
  assert(motor_h);
  motor_h->target_speed = target_speed_mps;
  s_target_update_count++;
}

/**
 * @brief 记录电机制动请求。
 * @param motor_h 电机基类视图，不得为 NULL。
 * @post 目标速度清零并递增制动计数。
 * @note 测试替身不访问硬件。
 */
void Dev_Motor_Base_Brake(motor_base_t *motor_h) {
  assert(motor_h);
  motor_h->target_speed = 0.0f;
  s_brake_count++;
}

/**
 * @brief 记录电机禁用请求。
 * @param motor_h 电机基类视图，不得为 NULL。
 * @post 目标速度清零并递增禁用计数。
 * @note 测试替身不访问硬件。
 */
void Dev_Motor_Base_Disable(motor_base_t *motor_h) {
  assert(motor_h);
  motor_h->target_speed = 0.0f;
  s_disable_count++;
}

/**
 * @brief 验证车体系 twist 转换、Policy 转发和 IMU 失效停车。
 * @return 0 表示通过。
 * @post body twist 转为世界方向并写入四轮目标；过期 IMU 触发制动和命令清零。
 */
int main(void) {
  imu_base_t imu = {0};
  motor_base_t motors[CHASSIS_OMNI_WHEEL_COUNT] = {0};
  motor_base_t *motor_h[CHASSIS_OMNI_WHEEL_COUNT] = {
    &motors[0],
    &motors[1],
    &motors[2],
    &motors[3],
  };
  fake_policy_t policy = {0};
  chassis_omni_t chassis = {0};

  Policy_Omni_Base_Init(&policy.base, "fake-policy", &s_fake_policy_ops);
  Chassis_Omni_Init(
    &chassis,
    "test-chassis",
    motor_h,
    &imu,
    &policy.base,
    100u
  );

  s_imu_available = true;
  s_imu_sample.sequence = 1u;
  s_imu_sample.valid_mask = IMU_CHANNEL_YAW;
  s_imu_sample.yaw_deg = 90.0f;
  s_imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW] = 1000u;

  chassis_twist_t twist = {
    .vx_body_mps = 3.0f,
    .vy_body_mps = 4.0f,
    .wz_radps = 0.5f,
  };
  Chassis_Base_SetMotion(&chassis.base, &twist);
  Chassis_Base_Update(&chassis.base, 1000u);

  const float pi = 3.14159265358979323846f;
  assert(policy.solve_count == 1u);
  assert(fabsf(policy.linear_speed_mps - 5.0f) < 0.001f);
  assert(fabsf(policy.direction_rad - (atan2f(4.0f, 3.0f) + pi / 2.0f)) < 0.001f);
  assert(fabsf(policy.yaw_rad - pi / 2.0f) < 0.001f);
  assert(fabsf(policy.angular_speed_radps - 0.5f) < 0.001f);
  assert(s_target_update_count == CHASSIS_OMNI_WHEEL_COUNT);
  for (uint8_t wheel = 0u; wheel < CHASSIS_OMNI_WHEEL_COUNT; wheel++) {
    assert(fabsf(motors[wheel].target_speed - (float)(wheel + 1u)) < 0.001f);
  }

  Chassis_Base_Update(&chassis.base, 1200u);
  assert(s_brake_count == CHASSIS_OMNI_WHEEL_COUNT);
  assert(chassis.command.vx_body_mps == 0.0f);
  assert(chassis.command.vy_body_mps == 0.0f);
  assert(chassis.command.wz_radps == 0.0f);

  Chassis_Base_EStop(&chassis.base);
  assert(s_disable_count == CHASSIS_OMNI_WHEEL_COUNT);
  return 0;
}
