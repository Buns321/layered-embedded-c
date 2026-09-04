//
// Created by Buns_ on 2026/9/1.
//

#include "../Inc/usart_controller.h"

/* ======================= 协议常量 ======================= */
#define BT_FRAME_HEADER_1   0xAAu
#define BT_FRAME_HEADER_2   0x55u

/* 12 位 ADC：中值 2048，死区 ±100，线性映射到 ±MAX_SPEED */
#define BT_ADC_MIDDLE   2048
#define BT_ADC_DEADZONE 100
#define BT_MAX_SPEED    1000

/* ======================= ADC 映射 ======================= */
/* ADC → 有符号速度：中值 2048 为 0，死区 ±100 内停车，之外线性映射到 ±MAX_SPEED */
static int32_t bt_map_adc(int32_t adc) {
  int32_t off = adc - BT_ADC_MIDDLE;                       /* -2048 ~ +2047 */
  if (off > BT_ADC_DEADZONE) {
    return (off - BT_ADC_DEADZONE) * BT_MAX_SPEED / (BT_ADC_MIDDLE - BT_ADC_DEADZONE);
  } else if (off < -BT_ADC_DEADZONE) {
    return (off + BT_ADC_DEADZONE) * BT_MAX_SPEED / (BT_ADC_MIDDLE - BT_ADC_DEADZONE);
  }
  return 0;   /* 死区内，停车 */
}

/* ======================= 帧解析 ======================= */
/* 校验 + 解析一帧 → 映射成运动指令 */
static void bt_parse_frame(usart_controller_t *self) {
  /* 和校验：前 8 字节之和的低 8 位 == 第 9 字节 */
  uint8_t sum = 0;
  for (uint8_t i = 0; i < BT_FRAME_LEN - 1; i++) sum += self->frame[i];
  if (sum != self->frame[BT_FRAME_LEN - 1]) return;   /* 校验失败，丢弃整帧 */

  /* 三路 ADC，小端：低字节在前 */
  int32_t adc1 = (int32_t)(self->frame[2] | ((uint16_t)self->frame[3] << 8));
  int32_t adc2 = (int32_t)(self->frame[4] | ((uint16_t)self->frame[5] << 8));
  int32_t adc3 = (int32_t)(self->frame[6] | ((uint16_t)self->frame[7] << 8));

  /* 映射到 Vx / Vy / ω */
  self->cmd.vx    = bt_map_adc(adc1);
  self->cmd.vy    = bt_map_adc(adc2);
  self->cmd.omega = bt_map_adc(adc3);
}

/* ======================= 驱动接口 ======================= */
void Dev_USART_Controller_Init(usart_controller_t *this, Platform_UART_Instance uart) {
  this->uart      = uart;
  this->state     = BT_STATE_SYNC_1;
  this->frame_idx = 0;
  this->cmd.vx    = 0;
  this->cmd.vy    = 0;
  this->cmd.omega = 0;
}

void Dev_USART_Controller_FeedByte(usart_controller_t *this, uint8_t byte) {
  switch (this->state) {
    case BT_STATE_SYNC_1:
      if (byte == BT_FRAME_HEADER_1) {
        this->frame[0] = byte;
        this->frame_idx = 1;
        this->state = BT_STATE_SYNC_2;
      }
      break;

    case BT_STATE_SYNC_2:
      if (byte == BT_FRAME_HEADER_2) {
        this->frame[1] = byte;
        this->frame_idx = 2;
        this->state = BT_STATE_DATA;
      } else if (byte == BT_FRAME_HEADER_1) {
        /* 0xAA 0xAA 0x55 这类重入，重新对齐到第二个帧头 */
        this->frame[0] = byte;
        this->frame_idx = 1;
      } else {
        this->state = BT_STATE_SYNC_1;
      }
      break;

    case BT_STATE_DATA:
      this->frame[this->frame_idx++] = byte;
      if (this->frame_idx >= BT_FRAME_LEN) {
        bt_parse_frame(this);
        this->state = BT_STATE_SYNC_1;
      }
      break;

    default:
      this->state = BT_STATE_SYNC_1;
      break;
  }
}

usart_controller_cmd_t Dev_USART_Controller_GetCmd(usart_controller_t *this) {
  return this->cmd;
}
