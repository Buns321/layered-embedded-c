/**
 * @file test_bsp_encoder.c
 * @brief BSP 模块（Encoder/GPIO/PWM/UART/I2C/DWT）宿主机单元测试。
 * @ingroup tests
 */

#include "bsp_dwt.h"
#include "bsp_encoder.h"
#include "bsp_gpio.h"
#include "bsp_i2c.h"
#include "bsp_mcu.h"
#include "bsp_pwm.h"
#include "bsp_uart.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_encoder_16bit_wrap_and_invert(void) {
  BSP_Stub_ResetAll();
  TIM_TypeDef tim3_inst = {.is_32bit = false};
  TIM_HandleTypeDef htim3 = {
      .Instance = &tim3_inst,
      .Init = {.Period = 0xFFFFU},
      .cnt = 65530U,
  };
  bsp_encoder_t enc;
  bsp_encoder_cfg_t cfg = {
      .htim = &htim3,
      .max_counts_per_period = 0U,
      .invert = false,
  };

  assert(BSP_Encoder_Init(&enc, &cfg) == BSP_OK);
  assert(htim3.started_encoder);
  assert(enc.bit_width == 16U);
  assert(BSP_Encoder_GetDelta(&enc) == 0);
  assert(BSP_Encoder_GetPosition(&enc) == 0);

  /* 正向回绕：65530 -> 4 => +10 counts */
  htim3.cnt = 4U;
  assert(BSP_Encoder_GetDelta(&enc) == 10);
  assert(BSP_Encoder_GetPosition(&enc) == 10);

  /* 反向回绕：4 -> 65530 => -10 counts */
  htim3.cnt = 65530U;
  assert(BSP_Encoder_GetDelta(&enc) == -10);
  assert(BSP_Encoder_GetPosition(&enc) == 0);

  /* invert = true 对增量和累计位置同时取反 */
  cfg.invert = true;
  htim3.cnt = 100U;
  assert(BSP_Encoder_Init(&enc, &cfg) == BSP_OK);
  assert(BSP_Encoder_GetDelta(&enc) == 0);
  htim3.cnt = 125U;
  assert(BSP_Encoder_GetDelta(&enc) == -25);
  assert(BSP_Encoder_GetPosition(&enc) == -25);

  /* Resync 保留 position_counts 并重建速度基线 */
  htim3.cnt = 300U;
  BSP_Encoder_Resync(&enc);
  assert(BSP_Encoder_GetPosition(&enc) == -25);
  assert(BSP_Encoder_GetDelta(&enc) == 0);

  /* ResetZero 同时清零硬件计数与累计位置 */
  BSP_Encoder_ResetZero(&enc);
  assert(htim3.cnt == 0U);
  assert(BSP_Encoder_GetPosition(&enc) == 0);
}

static void test_encoder_32bit_clamp_and_validation(void) {
  BSP_Stub_ResetAll();
  TIM_TypeDef tim2_inst = {.is_32bit = true};
  TIM_TypeDef tim4_inst = {.is_32bit = false};
  TIM_HandleTypeDef bad_htim = {.Instance = &tim4_inst, .Init = {.Period = 0xFFFFFFFFU}};
  TIM_HandleTypeDef htim2 = {.Instance = &tim2_inst, .Init = {.Period = 0xFFFFFFFFU}, .cnt = 10U};

  bsp_encoder_t enc;
  bsp_encoder_cfg_t bad_cfg = {.htim = &bad_htim};
  assert(BSP_Encoder_Init(NULL, &bad_cfg) == BSP_ERR_INVALID_ARG);
  assert(BSP_Encoder_Init(&enc, NULL) == BSP_ERR_INVALID_ARG);
  assert(BSP_Encoder_Init(&enc, &bad_cfg) == BSP_ERR_INVALID_ARG);

  bad_htim.Init.Period = 1000U;
  assert(BSP_Encoder_Init(&enc, &bad_cfg) == BSP_ERR_INVALID_ARG);

  BSP_Stub_SetEncoderStartStatus(HAL_ERROR);
  bsp_encoder_cfg_t cfg = {.htim = &htim2, .max_counts_per_period = 50U, .invert = false};
  assert(BSP_Encoder_Init(&enc, &cfg) == BSP_ERR_HAL);
  BSP_Stub_SetEncoderStartStatus(HAL_OK);

  assert(BSP_Encoder_Init(&enc, &cfg) == BSP_OK);
  assert(enc.bit_width == 32U);
  htim2.cnt = 0xFFFFFFF6U; /* -10 */
  assert(BSP_Encoder_GetDelta(&enc) == 0);
  htim2.cnt = 10U; /* +20 across 32-bit wrap */
  assert(BSP_Encoder_GetDelta(&enc) == 20);
  assert(!BSP_Encoder_HasOverflow(&enc));

  /* 正负超限夹紧并锁存 overflow */
  htim2.cnt = 200U; /* +190 -> clamp +50 */
  assert(BSP_Encoder_GetDelta(&enc) == 50);
  assert(BSP_Encoder_HasOverflow(&enc));
  BSP_Encoder_ClearOverflow(&enc);
  assert(!BSP_Encoder_HasOverflow(&enc));

  htim2.cnt = 0U; /* -200 -> clamp -50 */
  assert(BSP_Encoder_GetDelta(&enc) == -50);
  assert(BSP_Encoder_HasOverflow(&enc));
  assert(BSP_Encoder_GetPosition(&enc) == 20);
}

static void test_gpio_and_pwm(void) {
  BSP_Stub_ResetAll();
  GPIO_TypeDef gpio_d = {0};
  bsp_gpio_t valid_pin = {.port = &gpio_d, .pin = GPIO_PIN_12};
  bsp_gpio_t bad_null = {.port = NULL, .pin = GPIO_PIN_12};
  bsp_gpio_t bad_zero = {.port = &gpio_d, .pin = 0U};
  bsp_gpio_t bad_multi = {.port = &gpio_d, .pin = (uint16_t)(GPIO_PIN_0 | GPIO_PIN_1)};

  assert(BSP_GPIO_IsValid(valid_pin));
  assert(!BSP_GPIO_IsValid(bad_null));
  assert(!BSP_GPIO_IsValid(bad_zero));
  assert(!BSP_GPIO_IsValid(bad_multi));

  BSP_GPIO_Write(valid_pin, true);
  assert(BSP_GPIO_Read(valid_pin));
  BSP_GPIO_Toggle(valid_pin);
  assert(!BSP_GPIO_Read(valid_pin));
  BSP_GPIO_Write(bad_multi, true);
  assert(!BSP_GPIO_Read(bad_multi));

  TIM_TypeDef tim1_inst = {.is_32bit = false};
  TIM_HandleTypeDef htim1 = {.Instance = &tim1_inst, .arr = 4199U};
  bsp_pwm_t pwm = {.htim = &htim1, .channel = TIM_CHANNEL_2, .max_duty = 1000U};
  assert(BSP_PWM_Start(&pwm) == BSP_OK);
  assert((htim1.started_pwm_mask & (1U << 1U)) != 0U);
  assert(BSP_PWM_SetDuty(&pwm, 500U) == BSP_OK);
  assert(htim1.ccr[1] == (uint32_t)(((uint64_t)4199U * 500U) / 1000U));
  assert(BSP_PWM_SetDuty(&pwm, 2000U) == BSP_OK);
  assert(htim1.ccr[1] == 4199U);

  /* max_duty == 0 时按 ARR + 1 解释 */
  bsp_pwm_t pwm_raw = {.htim = &htim1, .channel = TIM_CHANNEL_1, .max_duty = 0U};
  assert(BSP_PWM_SetDuty(&pwm_raw, 4200U) == BSP_OK);
  assert(htim1.ccr[0] == 4199U);

  /* 64 位防溢出：ARR = 0x80000000, duty = max_duty / 2 */
  TIM_HandleTypeDef htim32 = {.Instance = &tim1_inst, .arr = 0x80000000U};
  bsp_pwm_t pwm_big = {.htim = &htim32, .channel = TIM_CHANNEL_3, .max_duty = 1000000U};
  assert(BSP_PWM_SetDuty(&pwm_big, 500000U) == BSP_OK);
  assert(htim32.ccr[2] == 0x40000000U);

  assert(BSP_PWM_Stop(&pwm) == BSP_OK);
  bsp_pwm_t bad_ch = {.htim = &htim1, .channel = 99U, .max_duty = 100U};
  assert(BSP_PWM_SetDuty(&bad_ch, 10U) == BSP_ERR_INVALID_ARG);
}

static uint32_t s_tx_done_calls;
static void *s_tx_done_last_ctx;

static void on_test_tx_done(void *ctx) {
  s_tx_done_calls++;
  s_tx_done_last_ctx = ctx;
}

static void test_uart_and_dwt(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();
  s_tx_done_calls = 0U;
  s_tx_done_last_ctx = NULL;

  BSP_DWT_Init();
  assert(BSP_DWT_CyclesToUs(1680U) == 10U);
  BSP_DWT_DelayUs(25U);
  assert(BSP_DWT_CyclesToUs(BSP_DWT_GetCycles()) == 25U);
  BSP_DelayMs(2U);
  assert(BSP_DWT_CyclesToUs(BSP_DWT_GetCycles()) == 2025U);

  /* 1. 验证 HAL_UART_STATE_READY (0x20) 下 IsTxBusy 为 false，BUSY_TX (0x21) 为 true */
  UART_HandleTypeDef huart2 = {.gState = HAL_UART_STATE_READY};
  bsp_uart_t uart2;
  int ctx_marker = 42;
  bsp_uart_cfg_t tx_cfg = {
      .huart = &huart2,
      .rx_mode = BSP_UART_RX_NONE,
      .on_tx_done = on_test_tx_done,
      .tx_ctx = &ctx_marker,
  };
  assert(BSP_UART_Init(&uart2, &tx_cfg) == BSP_OK);
  assert(BSP_UART_Init(&uart2, &tx_cfg) == BSP_ERR_BUSY);
  assert(!BSP_UART_IsTxBusy(&uart2));

  const uint8_t frame[] = {0xAAU, 0x55U, 0x01U, 0x02U};
  assert(BSP_UART_SendDMA(&uart2, frame, sizeof(frame)) == BSP_OK);
  assert(BSP_UART_IsTxBusy(&uart2));
  assert(BSP_UART_SendDMA(&uart2, frame, sizeof(frame)) == BSP_ERR_BUSY);

  huart2.gState = HAL_UART_STATE_READY;
  HAL_UART_TxCpltCallback(&huart2);
  assert(!BSP_UART_IsTxBusy(&uart2));
  assert(s_tx_done_calls == 1U);
  assert(s_tx_done_last_ctx == &ctx_marker);

  /* 2. 单字节中断接收 + 队列满溢出 + 错误重挂 */
  UART_HandleTypeDef huart3 = {.gState = HAL_UART_STATE_READY};
  uint8_t ring3[4];
  bsp_uart_t uart3;
  bsp_uart_cfg_t it_cfg = {
      .huart = &huart3,
      .rx_mode = BSP_UART_RX_IT_BYTE,
      .rx_ring_buf = ring3,
      .rx_ring_size = sizeof(ring3),
  };
  assert(BSP_UART_Init(&uart3, &it_cfg) == BSP_OK);
  assert(BSP_UART_StartRx(&uart3) == BSP_OK);

  const uint8_t rx_seq[] = {0x55U, 0x51U, 0x10U, 0x99U};
  for (uint32_t i = 0U; i < 4U; ++i) {
    *huart3.rx_buf = rx_seq[i];
    HAL_UART_RxCpltCallback(&huart3);
  }
  assert(uart3.rx_bytes == 4U);
  assert(uart3.rx_dropped_bytes == 1U);
  assert(BSP_UART_TakeResync(&uart3));
  assert(!BSP_UART_TakeResync(&uart3));

  uint8_t b = 0U;
  assert(BSP_UART_PopByte(&uart3, &b) && b == 0x55U);
  assert(BSP_UART_PopByte(&uart3, &b) && b == 0x51U);
  assert(BSP_UART_PopByte(&uart3, &b) && b == 0x10U);
  assert(!BSP_UART_PopByte(&uart3, &b));

  /* ISR 重挂失败后任务侧 EnsureRxArmed 恢复 */
  BSP_Stub_SetUartRxStatus(HAL_BUSY);
  HAL_UART_ErrorCallback(&huart3);
  assert((huart3.cleared_flags & 0x0FU) == 0x0FU);
  assert(uart3.rx_error_count == 1U);
  assert(BSP_UART_TakeResync(&uart3));
  assert(uart3.rearm_pending);

  BSP_Stub_SetUartRxStatus(HAL_OK);
  BSP_UART_EnsureRxArmed(&uart3);
  assert(!uart3.rearm_pending);

  /* 3. DMA_CIRCULAR ReceiveToIdle 增量与回绕切片 */
  DMA_HandleTypeDef dma_rx1 = {.Init = {.Mode = DMA_CIRCULAR}};
  UART_HandleTypeDef huart1 = {.gState = HAL_UART_STATE_READY, .hdmarx = &dma_rx1};
  uint8_t ring1[16];
  uint8_t dma1[8];
  bsp_uart_t uart1;
  bsp_uart_cfg_t dma_cfg = {
      .huart = &huart1,
      .rx_mode = BSP_UART_RX_DMA_IDLE,
      .rx_ring_buf = ring1,
      .rx_ring_size = sizeof(ring1),
      .rx_dma_buf = dma1,
      .rx_dma_size = sizeof(dma1),
  };
  assert(BSP_UART_Init(&uart1, &dma_cfg) == BSP_OK);
  assert(BSP_UART_StartRx(&uart1) == BSP_OK);
  assert((dma_rx1.disabled_it_mask & DMA_IT_HT) != 0U);

  dma1[0] = 10U;
  dma1[1] = 20U;
  dma1[2] = 30U;
  HAL_UARTEx_RxEventCallback(&huart1, 3U);
  dma1[3] = 40U;
  dma1[4] = 50U;
  dma1[5] = 60U;
  dma1[6] = 70U;
  dma1[7] = 80U;
  dma1[0] = 90U;
  dma1[1] = 100U;
  HAL_UARTEx_RxEventCallback(&huart1, 2U); /* 回绕：[3..8) + [0..2) */

  const uint8_t expected_dma[] = {10U, 20U, 30U, 40U, 50U, 60U, 70U, 80U, 90U, 100U};
  for (uint32_t i = 0U; i < sizeof(expected_dma); ++i) {
    assert(BSP_UART_PopByte(&uart1, &b) && b == expected_dma[i]);
  }
  assert(!BSP_UART_PopByte(&uart1, &b));

  /* 4. 验证静态槽位上限 BSP_UART_MAX_INSTANCES (6) */
  UART_HandleTypeDef extra_huarts[4] = {0};
  bsp_uart_t extra_uarts[4];
  for (uint32_t i = 0U; i < 3U; ++i) {
    bsp_uart_cfg_t c = {.huart = &extra_huarts[i], .rx_mode = BSP_UART_RX_NONE};
    assert(BSP_UART_Init(&extra_uarts[i], &c) == BSP_OK);
  }
  bsp_uart_cfg_t full_cfg = {.huart = &extra_huarts[3], .rx_mode = BSP_UART_RX_NONE};
  assert(BSP_UART_Init(&extra_uarts[3], &full_cfg) == BSP_ERR_NO_MEM);
  BSP_UART_ResetSlots();
}

static uint16_t s_seen_i2c_addr;
static uint16_t s_seen_mem_add_size;

static HAL_StatusTypeDef fake_i2c_probe(I2C_HandleTypeDef *hi2c, uint16_t dev_addr, uint32_t trials,
                                        uint32_t timeout_ms) {
  (void)hi2c;
  (void)trials;
  (void)timeout_ms;
  s_seen_i2c_addr = dev_addr;
  return HAL_OK;
}

static HAL_StatusTypeDef fake_i2c_mem(I2C_HandleTypeDef *hi2c, uint16_t dev_addr, uint16_t mem_addr,
                                      uint16_t mem_add_size, uint8_t *data, uint16_t size,
                                      uint32_t timeout_ms) {
  (void)hi2c;
  (void)mem_addr;
  (void)timeout_ms;
  s_seen_i2c_addr = dev_addr;
  s_seen_mem_add_size = mem_add_size;
  if (size > 0U && data != NULL) {
    data[0] = 0x68U;
  }
  return HAL_OK;
}

static void test_i2c(void) {
  BSP_Stub_ResetAll();
  BSP_Stub_I2C_SetHandlers(fake_i2c_probe, fake_i2c_mem, fake_i2c_mem);

  I2C_HandleTypeDef hi2c1 = {.Instance = (void *)1};
  bsp_i2c_t dev = {.hi2c = &hi2c1, .addr_7bit = 0x68U, .timeout_ms = 10U};
  assert(BSP_I2C_Probe(&dev, 3U) == BSP_OK);
  assert(s_seen_i2c_addr == (0x68U << 1U));

  uint8_t val = 0U;
  assert(BSP_I2C_MemRead(&dev, 0x75U, 1U, &val, 1U) == BSP_OK);
  assert(s_seen_mem_add_size == I2C_MEMADD_SIZE_8BIT);
  assert(val == 0x68U);

  const uint8_t tx_val = 0x01U;
  assert(BSP_I2C_MemWrite(&dev, 0x1000U, 2U, &tx_val, 1U) == BSP_OK);
  assert(s_seen_mem_add_size == I2C_MEMADD_SIZE_16BIT);

  assert(BSP_I2C_MemWrite(&dev, 0x10U, 3U, &tx_val, 1U) == BSP_ERR_INVALID_ARG);
  dev.addr_7bit = 0x80U;
  assert(BSP_I2C_Probe(&dev, 1U) == BSP_ERR_INVALID_ARG);
}

int main(void) {
  test_encoder_16bit_wrap_and_invert();
  test_encoder_32bit_clamp_and_validation();
  test_gpio_and_pwm();
  test_uart_and_dwt();
  test_i2c();
  (void)puts("test_bsp_encoder passed");
  return 0;
}
