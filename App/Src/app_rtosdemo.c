//
// Created by Buns_ on 2026/8/29.
//

#include "../Inc/app_rtosdemo.h"
#include "main.h"
#include <assert.h>

#include "../../DeviceDrivers/Inc/motor_tb6612.h"

void App_RTOSDemo_Task_Init(void* this, uint32_t msg_count, const char *name, Platform_GPIO_Port gpio_port, uint16_t gpio_pin, Platform_GPIO_Pin_State active_level) {
  assert(this);
  led_rtos_demo_t *self = (led_rtos_demo_t*) this;
  self->msg_count = msg_count;
  Dev_LED_GPIO_Init(&self->led_gpio, name, gpio_port, gpio_pin, active_level);
}

void App_RTOSDemo_RTOS_Init(void* this) {
  assert(this);
  led_rtos_demo_t *self = (led_rtos_demo_t*) this;
  self->led_queue = osMessageQueueNew(self->msg_count, sizeof(led_command_t), NULL);
  if (self->led_queue == NULL) Error_Handler();
}

void StartUSARTTask(void *argument) {
  for (;;) {
  led_rtos_demo_t *self = (led_rtos_demo_t*) argument;
  led_command_t cmd = LED_CMD_TOGGLE;
  osMessageQueuePut(self->led_queue, &cmd, 0, osWaitForever);
  osDelay(1000);
  }
}

void StartLEDTask(void *argument) {
  for (;;) {
  led_rtos_demo_t *self = (led_rtos_demo_t*) argument;
  led_command_t cmd = 0;
  if(osMessageQueueGet(self->led_queue, &cmd, NULL, osWaitForever) == osOK) {
    switch (cmd) {
      case LED_CMD_ON:     Dev_LED_Base_ON(&self->led_gpio.led_base);     break;
      case LED_CMD_OFF:    Dev_LED_Base_OFF(&self->led_gpio.led_base);    break;
      case LED_CMD_TOGGLE: Dev_LED_Base_Toggle(&self->led_gpio.led_base); break;
      default:             break;
    }
  }
  }
}

#include "../../DeviceDrivers/Base/Inc/motor_base.h"

static led_gpio_t test_led;
static motor_tb6612_t test_motor;

void StartTestTask(void *argument) {
  Dev_Motor_TB6612_Init(&test_motor, "motor", 10000, PLATFORM_TIM_TIM3, PLATFORM_TIM_CHANNEL_1,  PLATFORM_GPIO_PORT_B, 1u << 12, PLATFORM_GPIO_PORT_B, 1u << 13, PLATFORM_GPIO_PORT_B, 1u << 14);
  Dev_Motor_Base_Enable(&test_motor.motor_base);
  for (;;) {
    Dev_Motor_Base_SetSpeed(&test_motor.motor_base, 10000);
    // HAL_Delay(500);
    // Dev_Motor_Base_SetSpeed(&test_motor.motor_base, -5000);
    // HAL_Delay(500);
    // App_Blink(&test_led.led_base, 500);
  }
}
