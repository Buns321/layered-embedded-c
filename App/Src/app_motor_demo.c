//
// Created by Buns_ on 2026/8/25.
//

#include "../Inc/app_motor_demo.h"
#include "../../System/Inc/system_delay.h"
#include <assert.h>

void App_Motor_Demo(motor_base_t *this) {
  assert(this);
  Dev_Motor_Base_SetSpeed(this, 5000);
  System_Delay_Ms(2000);
  Dev_Motor_Base_SetSpeed(this, -5000);
  System_Delay_Ms(2000);
}
