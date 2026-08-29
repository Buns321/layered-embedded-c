//
// Created by Buns_ on 2026/8/29.
//

#include "../Inc/app_rtosdemo.h"
#include "main.h"
#include <assert.h>

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
