/**
 * @file chassis_omni.c
 * @brief 四轮全向底盘实现。
 * @details 本文件属于 Framework/Chassis 层，实现 chassis_omni_t 的虚表回调与装配校验，
 *          支持 CHASSIS_FRAME_BODY（车体系）与 CHASSIS_FRAME_WORLD（无头世界系）两种模式，
 *          并在世界系下对 IMU 偏航角执行有效性及超时保护。
 * @ingroup Framework_Chassis
 */

#include "chassis_omni.h"
#include "sys_angle.h"
#include "sys_math.h"
#include "sys_time_window.h"
#include <stddef.h>

/**
 * @brief 保存并清洗底盘期望速度命令（NaN/Inf 分量归零）。
 * @param[in,out] chassis_h 底盘基类指针（实际指向 chassis_omni_t 首成员）。
 * @param[in]     twist     期望速度命令指针。
 */
static void chassis_omni_setmotion(chassis_base_t *chassis_h, const chassis_twist_t *twist) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL || twist == NULL) {
    return;
  }
  // 对外部输入执行有限值过滤，防止 NaN/Inf 污染运动学逆解与 PID 积分器
  chassis->command.vx_body_mps = SYS_ISFINITE(twist->vx_body_mps) ? twist->vx_body_mps : 0.0f;
  chassis->command.vy_body_mps = SYS_ISFINITE(twist->vy_body_mps) ? twist->vy_body_mps : 0.0f;
  chassis->command.wz_radps = SYS_ISFINITE(twist->wz_radps) ? twist->wz_radps : 0.0f;
}

/**
 * @brief 常规制动停机：清零命令并将四轮目标速度置 0 后请求电机主动制动。
 * @param[in,out] chassis_h 底盘基类指针（实际指向 chassis_omni_t 首成员）。
 */
static void chassis_omni_stop(chassis_base_t *chassis_h) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL) {
    return;
  }
  chassis->command = (chassis_twist_t){0};
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    Dev_Motor_Base_SetTargetSpeed(chassis->motors[i], 0.0f);
    Dev_Motor_Base_Brake(chassis->motors[i]);
  }
}

/**
 * @brief 紧急断电停机：清零命令并立即禁用四路电机输出（释放使能/STBY）。
 * @param[in,out] chassis_h 底盘基类指针（实际指向 chassis_omni_t 首成员）。
 */
static void chassis_omni_estop(chassis_base_t *chassis_h) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL) {
    return;
  }
  chassis->command = (chassis_twist_t){0};
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    Dev_Motor_Base_Disable(chassis->motors[i]);
  }
}

/**
 * @brief 周期运动学逆解并下发四轮目标线速度；WORLD 模式下校验 IMU 偏航角新鲜度。
 * @param[in,out] chassis_h 底盘基类指针（实际指向 chassis_omni_t 首成员）。
 * @param[in]     now_ms    当前系统单调毫秒时间戳（单位：ms）。
 */
static void chassis_omni_update(chassis_base_t *chassis_h, uint32_t now_ms) {
  chassis_omni_t *chassis = (chassis_omni_t *)chassis_h;
  if (chassis == NULL || chassis->kinematics == NULL) {
    return;
  }

  float yaw_rad = 0.0f;
  if (chassis->frame == CHASSIS_FRAME_WORLD) {
    imu_sample_t imu_sample = {0};
    // 世界系（无头模式）强依赖有效且未过期的偏航角；若 IMU 缺失、通道无效或超时，
    // 立即制动停机以防止错误朝向解算导致失控
    if (chassis->imu == NULL || !Dev_IMU_Base_GetSample(chassis->imu, &imu_sample) ||
        (imu_sample.valid_mask & IMU_CHANNEL_YAW) == 0U ||
        Sys_Time_IsExpiredMs(now_ms, imu_sample.channel_update_ms[IMU_CHANNEL_INDEX_YAW],
                             chassis->imu_timeout_ms)) {
      chassis_omni_stop(chassis_h);
      return;
    }
    yaw_rad = Sys_Angle_DegToRad(imu_sample.yaw_deg);
  }

  // 根据当前坐标系与偏航角执行四轮全向运动学逆解，得到各轮目标线速度（m/s）
  float wheel_mps[CHASSIS_OMNI_WHEEL_COUNT] = {0};
  Kinematics_Omni_Solve(chassis->kinematics, chassis->command.vx_body_mps,
                        chassis->command.vy_body_mps, chassis->command.wz_radps, chassis->frame,
                        yaw_rad, wheel_mps);

  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    Dev_Motor_Base_SetTargetSpeed(chassis->motors[i], wheel_mps[i]);
  }
}

/** @brief 四轮全向底盘只读虚表实例。 */
static const chassis_ops_t s_chassis_ops_omni = {
    .set_motion = chassis_omni_setmotion,
    .update = chassis_omni_update,
    .stop = chassis_omni_stop,
    .estop = chassis_omni_estop,
};

bool Chassis_Omni_Init(chassis_omni_t *chassis, const chassis_omni_cfg_t *cfg) {
  if (chassis == NULL || cfg == NULL || cfg->name == NULL || cfg->motors == NULL ||
      cfg->kinematics == NULL) {
    return false;
  }
  if (cfg->frame != CHASSIS_FRAME_BODY && cfg->frame != CHASSIS_FRAME_WORLD) {
    return false;
  }
  // WORLD 模式必须绑定有效 IMU 且配置非零超时阈值
  if (cfg->frame == CHASSIS_FRAME_WORLD && (cfg->imu == NULL || cfg->imu_timeout_ms == 0U)) {
    return false;
  }

  // 校验四路电机句柄均非 NULL 且互不重复，避免同一电机被多轮槽位覆盖控制
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    if (cfg->motors[i] == NULL) {
      return false;
    }
    for (uint8_t j = 0U; j < i; ++j) {
      if (cfg->motors[i] == cfg->motors[j]) {
        return false;
      }
    }
  }

  // 全部校验通过后再写入 chassis，确保失败时不会留下半初始化状态
  Chassis_Base_Init(&chassis->base, cfg->name, &s_chassis_ops_omni);
  for (uint8_t i = 0U; i < CHASSIS_OMNI_WHEEL_COUNT; ++i) {
    chassis->motors[i] = cfg->motors[i];
  }
  chassis->kinematics = cfg->kinematics;
  chassis->imu = cfg->imu;
  chassis->frame = cfg->frame;
  chassis->command = (chassis_twist_t){0};
  chassis->imu_timeout_ms = cfg->imu_timeout_ms;
  return true;
}

void Chassis_Omni_SetMotion(chassis_omni_t *chassis, const chassis_twist_t *twist) {
  if (chassis == NULL) {
    return;
  }
  Chassis_Base_SetMotion(&chassis->base, twist);
}
