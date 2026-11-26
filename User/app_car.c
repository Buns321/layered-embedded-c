/**
 * @file app_car.c
 * @brief F407 四轮全向小车应用层入口实现（零 HAL/BSP/FreeRTOS 依赖）。
 * @details 仅通过不透明指针 car_omni_t* 驱动整车门面，所有硬件句柄与
 *          RTOS 任务调度均由 board_cfg 与 rtos_tasks 隔离承担。
 */

#include "app_car.h"
#include "board_cfg.h"
#include "car_omni.h"
#include "sys_log.h"
#include <stddef.h>

/** @brief 整车门面静态实例句柄（由 Board_InitCar 返回，生命周期贯穿全程）。 */
static car_omni_t *s_car_h;

bool App_Car_Boot(void) {
  // 先启动 DWT 高精度计时与可选 RTT 日志后端，确保后续初始化阶段可输出诊断日志
  if (!Board_BootServices()) {
    return false;
  }

  // 完成四轮电机、编码器、IMU、遥控与遥测的静态装配
  s_car_h = Board_InitCar();
  if (s_car_h == NULL) {
    SYS_LOG_ERROR("APP", "car boot failed");
    return false;
  }

  SYS_LOG_INFO("APP", "car initialized");
  return true;
}

void App_Car_StepInput(uint32_t now_ms) {
  // 解析串口字节流中的 IMU 姿态与遥控器帧，并在有效新帧到达时喂狗
  Car_Omni_ProcessInputs(s_car_h, now_ms);
}

void App_Car_StepControl(uint32_t now_ms) {
  // 先将最新遥控归一化摇杆映射为底盘期望速度（含超时/死区归零保护）
  Car_Omni_DriveFromRC(s_car_h, now_ms);
  // 再执行全向运动学逆解与四轮编码器 PID 闭环输出
  Car_Omni_StepControl(s_car_h, now_ms);
}

void App_Car_StepService(uint32_t now_ms) {
  // 轮询节点离线看门狗，并按周期发送 JustFloat/FireWater 遥测与状态日志
  Car_Omni_StepService(s_car_h, now_ms);
}
