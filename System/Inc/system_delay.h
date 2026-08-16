//
// Created by Buns_ on 2026/8/16.
//

#ifndef LAYERED_EMBEDDED_C_SYSTEM_DELAY_H
#define LAYERED_EMBEDDED_C_SYSTEM_DELAY_H

#include <stdint.h>

void System_Delay_Init(void);
void System_Delay_Ms(uint32_t ms);
void System_Delay_Us(uint32_t us);

#endif //LAYERED_EMBEDDED_C_SYSTEM_DELAY_H
