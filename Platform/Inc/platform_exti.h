//
// Created by Buns_ on 2026/8/27.
//

#ifndef LAYERED_EMBEDDED_C_PLATFORM_EXTI_H
#define LAYERED_EMBEDDED_C_PLATFORM_EXTI_H

#include <stdint.h>

typedef void (*Platform_EXTI_Callback)(uint16_t GPIO_Pin, void* context);

typedef struct {
  Platform_EXTI_Callback exit_cb;
  void *context;
} platform_exti_callbacks_t;

void Platform_EXTI_RegisterCallbacks(uint8_t GPIO_Pin, const platform_exti_callbacks_t *cb);

#endif //LAYERED_EMBEDDED_C_PLATFORM_EXTI_H
