//
// Created by Buns_ on 2026/8/29.
//

#ifndef LAYERED_EMBEDDED_C_APP_RTOSDEMO_H
#define LAYERED_EMBEDDED_C_APP_RTOSDEMO_H

#include "cmsis_os2.h"

#include "../../DeviceDrivers/Inc/led_gpio.h"
#include "../../DeviceDrivers/Inc/usart_pingpong.h"

typedef struct {
  led_gpio_t led_gpio;
  osMessageQueueId_t led_queue;
  uint32_t msg_count;
} led_rtos_demo_t;

typedef enum {
  LED_CMD_OFF = 0U,
  LED_CMD_ON,
  LED_CMD_TOGGLE,
} led_command_t;

void App_RTOSDemo_Task_Init(void* this, uint32_t msg_count, const char *name, Platform_GPIO_Port gpio_port, uint16_t gpio_pin, Platform_GPIO_Pin_State active_level);
void App_RTOSDemo_RTOS_Init(void* this);
void StartUSARTTask(void *argument);
void StartLEDTask(void *argument);

#endif //LAYERED_EMBEDDED_C_APP_RTOSDEMO_H
