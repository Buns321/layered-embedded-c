//
// Created by Buns_ on 2026/8/15.
//

#ifndef LAYERED_EMBEDDED_C_LED_BASE_H
#define LAYERED_EMBEDDED_C_LED_BASE_H

#include <stdint.h>

typedef enum {
  DEV_LED_OFF = 0u,
  DEV_LED_ON,
}Dev_LED_State;

struct led_base;

typedef struct led_ops {
  void (*on)(struct led_base *this);
  void (*off)(struct led_base *this);
  void (*set_brightness)(struct led_base *this, uint16_t brightness);
}led_ops_t;

typedef struct led_base {
  const led_ops_t *ops;
  const char      *name;
  Dev_LED_State   is_on;
}led_base_t;

void Dev_LED_Base_Init(led_base_t *this, const char *name, const led_ops_t *ops);
Dev_LED_State Dev_LED_Base_Read_Status(const led_base_t *this);
void Dev_LED_Base_SetBrightness(led_base_t *this, uint16_t brightness);
void Dev_LED_Base_ON(led_base_t *this);
void Dev_LED_Base_OFF(led_base_t *this);
void Dev_LED_Base_Toggle(led_base_t *this);

#endif //LAYERED_EMBEDDED_C_LED_BASE_H
