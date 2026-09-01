//
// Created by Buns_ on 2026/8/31.
//

#ifndef LAYERED_EMBEDDED_C_ALGORITHM_OMNI_H
#define LAYERED_EMBEDDED_C_ALGORITHM_OMNI_H

/**
 * @brief 世界系全向轮逆运动学（哨兵式）
 *
 * 输入三个解耦的运动量 + 底盘朝向，输出 4 个轮子的目标线速度：
 *
 * @param omega   : 自转角速度（rad/s，正 = 逆时针）
 * @param speed   : 平移速度大小（单位自定，建议"编码器计数/控制周期"）
 * @param dir     : 平移方向（rad，世界系，0 = 场地 x 轴）
 * @param yaw     : 底盘当前朝向（rad，世界系，IMU 提供，已减零点偏移）
 *
 * @retval out[4] : 4 个轮子的目标速度（和 speed 同单位）
 *
 * 标定常数（theta/sign/L）在 .c 文件里，装好车实测后填入。
 */
void Algorithm_Omni_Kinematics_Update(float speed, float dir, float yaw, float omega, float out[4]);

#endif //LAYERED_EMBEDDED_C_ALGORITHM_OMNI_H
