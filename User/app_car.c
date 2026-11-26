/**
 * @file app_car.c
 * @brief F407 四轮全向小车应用层入口实现（零 HAL/BSP/FreeRTOS 依赖）。
 */

#include "app_car.h"
#include "board_cfg.h"
#include "car_omni.h"
#include "sys_log.h"
#include <stddef.h>

static car_omni_t *s_car_h;

bool App_Car_Boot(void) {
  if (!Board_BootServices()) {
    return false;
  }
  s_car_h = Board_InitCar();
  if (s_car_h == NULL) {
    SYS_LOG_ERROR("APP", "car boot failed");
    return false;
  }
  SYS_LOG_INFO("APP", "car initialized");
  return true;
}

void App_Car_StepInput(uint32_t now_ms) { Car_Omni_ProcessInputs(s_car_h, now_ms); }

void App_Car_StepControl(uint32_t now_ms) {
  Car_Omni_DriveFromRC(s_car_h, now_ms);
  Car_Omni_StepControl(s_car_h, now_ms);
}

void App_Car_StepService(uint32_t now_ms) { Car_Omni_StepService(s_car_h, now_ms); }
