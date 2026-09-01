//
// Created by Buns_ on 2026/8/31.
//

#include "../Inc/algorithm_omni.h"
#include <math.h>
#include <stdint.h>

#define PI 3.14159265358979323846f

/* 安装角：4 个轮子滚动方向 与 车头方向 的夹角（X 型 = ±45°） */
static const float WHEEL_THETA[4] = {
  -PI / 4.0f,        // 轮0：前左
  -3.0f * PI / 4.0f, // 轮1：前右
  +PI / 4.0f,        // 轮2：后左
  +3.0f * PI / 4.0f, // 轮3：后右
};

/* 方向符号：自转贡献的正负。标定时翻这个，别改 theta */
static const float WHEEL_SIGN[4] = { +1.0f, -1.0f, +1.0f, -1.0f };

/* 轮子到车体中心的距离（米） */
static const float WHEEL_DISTANCE = 0.10f;

/* ================================================================ */


void Algorithm_Omni_Kinematics_Update(float speed, float dir, float yaw, float omega, float out[4])
{
  for (uint8_t i = 0; i < 4; i++)
  {
    // 世界系轮向角：安装角 + 底盘朝向
    float alpha = WHEEL_THETA[i] + yaw;

    // 平移分量：平移速度 * cosf(平移方向 - 轮向角)
    float trans = speed * cosf(dir - alpha);

    // 自转分量：自转角速度 * 自传半径 * 方向符号
    float rotate = omega * WHEEL_DISTANCE * WHEEL_SIGN[i];

    // 输出：平移分量 + 自转分量
    out[i] = trans + rotate;
  }
}
