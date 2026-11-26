/**
 * @file kinematics_omni.h
 * @brief 四轮全向底盘扁平运动学解算（支持车体系零三角运算与世界系无头模式）。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          在 Init 阶段预计算各轮安装角的正余弦，运行期支持车体系（CHASSIS_FRAME_BODY）
 *          直接映射与世界系（CHASSIS_FRAME_WORLD）按 -yaw_rad 旋转后逆解四轮线速度。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_KINEMATICS_OMNI_H
#define LAYERED_CAR_KINEMATICS_OMNI_H

#include <stdbool.h>

/** @brief 四轮全向底盘轮数常量（固定为 4 轮）。 */
#define KINEMATICS_OMNI_WHEELS 4U

/**
 * @brief 速度命令所在参考坐标系枚举。
 */
typedef enum chassis_frame_e {
  CHASSIS_FRAME_BODY = 0U, /**< 车体系：x 指向车头，y 指向左侧，不需要 IMU yaw。 */
  CHASSIS_FRAME_WORLD      /**< 世界系（无头模式）：先按 -yaw_rad 旋至车体系后再解算。 */
} chassis_frame_t;

/**
 * @brief 四轮全向运动学配置（wheel_sign 为 0 时默认 +1.0f，否则须为 ±1.0f）。
 */
typedef struct kinematics_omni_cfg_s {
  float wheel_distance_m; /**< 轮心到车体旋转中心距离 R（单位：m，须 > 0）。 */
  float wheel_theta_rad[KINEMATICS_OMNI_WHEELS]; /**< 各轮滚动正方向相对车头 x
                                                    轴的夹角（单位：rad，逆时针为正）。 */
  float wheel_sign[KINEMATICS_OMNI_WHEELS]; /**< 各轮安装方向符号（0.0f 默认映射为 +1.0f，否则须为
                                               +1.0f 或 -1.0f）。 */
} kinematics_omni_cfg_t;

/**
 * @brief 四轮全向运动学已预计算实例。
 */
typedef struct kinematics_omni_s {
  float wheel_distance_m;                   /**< 轮心到车体旋转中心距离 R（单位：m）。 */
  float cos_theta[KINEMATICS_OMNI_WHEELS];  /**< 预计算的 cos(wheel_theta_rad[i])（无量纲）。 */
  float sin_theta[KINEMATICS_OMNI_WHEELS];  /**< 预计算的 sin(wheel_theta_rad[i])（无量纲）。 */
  float wheel_sign[KINEMATICS_OMNI_WHEELS]; /**< 归一化后的各轮方向符号（+1.0f 或 -1.0f）。 */
} kinematics_omni_t;

/**
 * @brief 校验参数并预计算四轮安装角的正余弦（失败返回 false 且不修改 kin）。
 * @param[out] kin 待初始化的运动学实例指针（非 NULL）。
 * @param[in]  cfg 运动学机械配置指针（非 NULL）。
 * @return 初始化是否成功。
 * @retval true  机械参数合法，正余弦与方向符号已预计算写入 kin。
 * @retval false 参数为空、轮距 <= 0、角度非有限或 wheel_sign 非 {0, +1, -1}（kin 保持原状）。
 */
bool Kinematics_Omni_Init(kinematics_omni_t *kin, const kinematics_omni_cfg_t *cfg);

/**
 * @brief 解算四轮目标线速度 out_wheel_mps（单位 m/s）；任一有效输入或结果非有限时全部清零。
 * @param[in]  kin           已初始化的运动学实例指针（非 NULL）。
 * @param[in]  vx_mps        期望 X 轴线速度（单位：m/s，须为有限值）。
 * @param[in]  vy_mps        期望 Y 轴线速度（单位：m/s，须为有限值）。
 * @param[in]  wz_radps      期望绕 Z 轴角速度（单位：rad/s，逆时针为正，须为有限值）。
 * @param[in]  frame         速度命令参考坐标系（CHASSIS_FRAME_BODY 或 CHASSIS_FRAME_WORLD）。
 * @param[in]  yaw_rad       车体相对世界系偏航角（单位：rad，仅 CHASSIS_FRAME_WORLD
 * 下校验并使用）。
 * @param[out] out_wheel_mps 四轮目标线速度输出数组（单位：m/s，非 NULL，长度
 * KINEMATICS_OMNI_WHEELS）。
 * @pre kin 须已通过 Kinematics_Omni_Init() 成功初始化。
 * @note 车体系下不校验 yaw_rad（允许传 0.0f）；若任一输入或中间结果非法，四轮输出原子清零。
 */
void Kinematics_Omni_Solve(const kinematics_omni_t *kin, float vx_mps, float vy_mps, float wz_radps,
                           chassis_frame_t frame, float yaw_rad,
                           float out_wheel_mps[KINEMATICS_OMNI_WHEELS]);

#endif /* LAYERED_CAR_KINEMATICS_OMNI_H */

/** @} */
