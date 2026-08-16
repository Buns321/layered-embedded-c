//
// Created by Buns_ on 2026/8/16.
//

#include "../Inc/app_blink.h"
#include "../../System/Inc/system_delay.h"
#include <assert.h>

void App_Blink(led_base_t *this, uint32_t delay_ms) {
  assert(this);
  Dev_LED_Base_Toggle(this);
  System_Delay_Ms(delay_ms);
}
