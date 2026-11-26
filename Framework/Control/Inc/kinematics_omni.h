/**
 * @file kinematics_omni.h
 * @brief 四轮全向底盘扁平运动学解算（支持车体系零三角运算与世界系无头模式）。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_KINEMATICS_OMNI_H
#define LAYERED_CAR_KINEMATICS_OMNI_H

#include <stdbool.h>

#define KINEMATICS_OMNI_WHEELS 4U

/**
 * @brief 速度命令所在参考坐标系。
 */
typedef enum chassis_frame_e {
  CHASSIS_FRAME_BODY = 0U, /* 车体系：x 车头，y 左侧，不需要 IMU yaw */
  CHASSIS_FRAME_WORLD      /* 世界系（无头模式）：按 -yaw_rad 旋至车体系后解算 */
} chassis_frame_t;

/**
 * @brief 四轮全向运动学配置（wheel_sign 为 0 时默认 +1.0f，否则须为 ±1.0f）。
 */
typedef struct kinematics_omni_cfg_s {
  float wheel_distance_m;
  float wheel_theta_rad[KINEMATICS_OMNI_WHEELS];
  float wheel_sign[KINEMATICS_OMNI_WHEELS];
} kinematics_omni_cfg_t;

/**
 * @brief 四轮全向运动学已预计算实例。
 */
typedef struct kinematics_omni_s {
  float wheel_distance_m;
  float cos_theta[KINEMATICS_OMNI_WHEELS];
  float sin_theta[KINEMATICS_OMNI_WHEELS];
  float wheel_sign[KINEMATICS_OMNI_WHEELS];
} kinematics_omni_t;

/**
 * @brief 校验参数并预计算四轮安装角的正余弦（失败返回 false 且不修改 kin）。
 */
bool Kinematics_Omni_Init(kinematics_omni_t *kin, const kinematics_omni_cfg_t *cfg);

/**
 * @brief 解算四轮目标线速度 out_wheel_mps（单位 m/s）；任一有效输入或结果非有限时全部清零。
 */
void Kinematics_Omni_Solve(const kinematics_omni_t *kin, float vx_mps, float vy_mps, float wz_radps,
                           chassis_frame_t frame, float yaw_rad,
                           float out_wheel_mps[KINEMATICS_OMNI_WHEELS]);

/** @} */

#endif /* LAYERED_CAR_KINEMATICS_OMNI_H */
