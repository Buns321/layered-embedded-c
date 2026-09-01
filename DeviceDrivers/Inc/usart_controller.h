//
// Created by Buns_ on 2026/9/1.
//

#ifndef LAYERED_EMBEDDED_C_USART_CONTROLLER_H
#define LAYERED_EMBEDDED_C_USART_CONTROLLER_H

#include "../../Platform/Inc/platform_usart.h"
#include <stdint.h>

/* 蓝牙遥控协议帧长：帧头2 + 三路ADC(各2字节) + 校验1 = 9 */
#define BT_FRAME_LEN 9u

/* 蓝牙遥控解析出的运动指令 */
typedef struct {
  int32_t vx;      /* 前后速度，有符号 */
  int32_t vy;      /* 左右速度，有符号 */
  int32_t omega;   /* 自转角速度，有符号 */
} usart_controller_cmd_t;

typedef enum {
  BT_STATE_SYNC_1 = 0U,   /* 等帧头 0xAA */
  BT_STATE_SYNC_2,        /* 等帧头 0x55 */
  BT_STATE_DATA,          /* 收剩余 7 字节(6 ADC + 1 校验) */
} usart_controller_state_t;

typedef struct {
  Platform_UART_Instance uart;
  usart_controller_state_t state;
  uint8_t frame[BT_FRAME_LEN];
  uint8_t frame_idx;
  volatile usart_controller_cmd_t cmd;   /* 解析出的最新运动指令 */
} usart_controler_t;

void Dev_USART_Controller_Init(usart_controler_t *this, Platform_UART_Instance uart);
void Dev_USART_Controller_FeedByte(usart_controler_t *this, uint8_t byte);
usart_controller_cmd_t Dev_USART_Controller_GetCmd(usart_controler_t *this);

#endif //LAYERED_EMBEDDED_C_USART_CONTROLLER_H
