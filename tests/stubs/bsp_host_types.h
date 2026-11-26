/**
 * @file bsp_host_types.h
 * @brief 宿主机单元测试使用的 STM32 HAL 与 Cortex-M 类型及桩声明。
 * @ingroup tests
 */

#ifndef LAYERED_CAR_BSP_HOST_TYPES_H
#define LAYERED_CAR_BSP_HOST_TYPES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum hal_status_e {
  HAL_OK = 0x00U,
  HAL_ERROR = 0x01U,
  HAL_BUSY = 0x02U,
  HAL_TIMEOUT = 0x03U,
} HAL_StatusTypeDef;

typedef enum gpio_pin_state_e {
  GPIO_PIN_RESET = 0U,
  GPIO_PIN_SET = 1U,
} GPIO_PinState;

#define GPIO_PIN_0 ((uint16_t)0x0001U)
#define GPIO_PIN_1 ((uint16_t)0x0002U)
#define GPIO_PIN_2 ((uint16_t)0x0004U)
#define GPIO_PIN_3 ((uint16_t)0x0008U)
#define GPIO_PIN_4 ((uint16_t)0x0010U)
#define GPIO_PIN_5 ((uint16_t)0x0020U)
#define GPIO_PIN_6 ((uint16_t)0x0040U)
#define GPIO_PIN_7 ((uint16_t)0x0080U)
#define GPIO_PIN_8 ((uint16_t)0x0100U)
#define GPIO_PIN_9 ((uint16_t)0x0200U)
#define GPIO_PIN_10 ((uint16_t)0x0400U)
#define GPIO_PIN_11 ((uint16_t)0x0800U)
#define GPIO_PIN_12 ((uint16_t)0x1000U)
#define GPIO_PIN_13 ((uint16_t)0x2000U)
#define GPIO_PIN_14 ((uint16_t)0x4000U)
#define GPIO_PIN_15 ((uint16_t)0x8000U)
#define GPIO_PIN_All ((uint16_t)0xFFFFU)

typedef struct GPIO_TypeDef_s {
  uint32_t odr;
  uint32_t idr;
} GPIO_TypeDef;

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);
GPIO_PinState HAL_GPIO_ReadPin(const GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

#define DMA_NORMAL 0x00000000U
#define DMA_CIRCULAR 0x00000100U
#define DMA_IT_HT 0x00000008U

typedef struct DMA_InitTypeDef_s {
  uint32_t Mode;
} DMA_InitTypeDef;

typedef struct DMA_HandleTypeDef_s {
  DMA_InitTypeDef Init;
  uint32_t disabled_it_mask;
} DMA_HandleTypeDef;

#define __HAL_DMA_DISABLE_IT(hdma, it)                                                             \
  do {                                                                                             \
    if ((hdma) != NULL) {                                                                          \
      (hdma)->disabled_it_mask |= (it);                                                            \
    }                                                                                              \
  } while (0)

typedef struct TIM_TypeDef_s {
  bool is_32bit;
} TIM_TypeDef;

typedef struct TIM_InitTypeDef_s {
  uint32_t Period;
} TIM_InitTypeDef;

typedef struct TIM_HandleTypeDef_s {
  TIM_TypeDef *Instance;
  TIM_InitTypeDef Init;
  uint32_t cnt;
  uint32_t arr;
  uint32_t ccr[4];
  uint32_t started_pwm_mask;
  bool started_encoder;
} TIM_HandleTypeDef;

#define TIM_CHANNEL_1 0x00000000U
#define TIM_CHANNEL_2 0x00000004U
#define TIM_CHANNEL_3 0x00000008U
#define TIM_CHANNEL_4 0x0000000CU
#define TIM_CHANNEL_ALL 0x0000003CU

#define IS_TIM_32B_COUNTER_INSTANCE(inst) (((inst) != NULL) && (inst)->is_32bit)
#define __HAL_TIM_GET_AUTORELOAD(htim) ((htim)->arr)
#define __HAL_TIM_GET_COUNTER(htim) ((htim)->cnt)
#define __HAL_TIM_SET_COUNTER(htim, val)                                                           \
  do {                                                                                             \
    (htim)->cnt = (val);                                                                           \
  } while (0)

static inline void bsp_stub_tim_set_compare(TIM_HandleTypeDef *htim, uint32_t ch, uint32_t val) {
  uint32_t idx = ch >> 2U;
  if (htim != NULL && idx < 4U) {
    htim->ccr[idx] = val;
  }
}

#define __HAL_TIM_SET_COMPARE(htim, ch, val) bsp_stub_tim_set_compare((htim), (ch), (val))

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *htim, uint32_t Channel);
HAL_StatusTypeDef HAL_TIM_Encoder_Stop(TIM_HandleTypeDef *htim, uint32_t Channel);

typedef enum hal_uart_state_e {
  HAL_UART_STATE_RESET = 0x00U,
  HAL_UART_STATE_READY = 0x20U,
  HAL_UART_STATE_BUSY = 0x24U,
  HAL_UART_STATE_BUSY_TX = 0x21U,
  HAL_UART_STATE_BUSY_RX = 0x22U,
  HAL_UART_STATE_BUSY_TX_RX = 0x23U,
  HAL_UART_STATE_TIMEOUT = 0xA0U,
  HAL_UART_STATE_ERROR = 0xE0U,
} HAL_UART_StateTypeDef;

typedef struct UART_HandleTypeDef_s {
  void *Instance;
  DMA_HandleTypeDef *hdmarx;
  DMA_HandleTypeDef *hdmatx;
  volatile uint32_t gState;
  volatile uint32_t RxState;
  volatile uint32_t ErrorCode;
  uint8_t *rx_buf;
  uint16_t rx_size;
  const uint8_t *last_tx_data;
  uint16_t last_tx_len;
  uint8_t last_tx_copy[16];
  uint8_t tx_history[64];
  uint16_t tx_history_len;
  uint32_t tx_blocking_calls;
  uint32_t tx_dma_calls;
  uint32_t rx_it_calls;
  uint32_t rx_dma_idle_calls;
  uint32_t cleared_flags;
} UART_HandleTypeDef;

#define __HAL_UART_CLEAR_OREFLAG(huart)                                                            \
  do {                                                                                             \
    if ((huart) != NULL) {                                                                         \
      (huart)->cleared_flags |= 0x01U;                                                             \
    }                                                                                              \
  } while (0)
#define __HAL_UART_CLEAR_FEFLAG(huart)                                                             \
  do {                                                                                             \
    if ((huart) != NULL) {                                                                         \
      (huart)->cleared_flags |= 0x02U;                                                             \
    }                                                                                              \
  } while (0)
#define __HAL_UART_CLEAR_NEFLAG(huart)                                                             \
  do {                                                                                             \
    if ((huart) != NULL) {                                                                         \
      (huart)->cleared_flags |= 0x04U;                                                             \
    }                                                                                              \
  } while (0)
#define __HAL_UART_CLEAR_PEFLAG(huart)                                                             \
  do {                                                                                             \
    if ((huart) != NULL) {                                                                         \
      (huart)->cleared_flags |= 0x08U;                                                             \
    }                                                                                              \
  } while (0)

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, const uint8_t *pData, uint16_t Size,
                                    uint32_t Timeout);
HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, const uint8_t *pData,
                                        uint16_t Size);
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart, uint8_t *pData,
                                               uint16_t Size);

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef *huart);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);

#define I2C_MEMADD_SIZE_8BIT 0x00000001U
#define I2C_MEMADD_SIZE_16BIT 0x00000010U

typedef struct I2C_HandleTypeDef_s {
  void *Instance;
} I2C_HandleTypeDef;

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                        uint32_t Trials, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                    uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                   uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                   uint16_t Size, uint32_t Timeout);

typedef struct bsp_stub_coredebug_s {
  uint32_t DEMCR;
} bsp_stub_coredebug_t;

typedef struct bsp_stub_dwt_s {
  uint32_t CTRL;
  uint32_t CYCCNT;
} bsp_stub_dwt_t;

typedef struct bsp_stub_scb_s {
  uint32_t CFSR;
  uint32_t HFSR;
  uint32_t MMFAR;
  uint32_t BFAR;
} bsp_stub_scb_t;

#define CoreDebug_DEMCR_TRCENA_Msk (1UL << 24)
#define DWT_CTRL_CYCCNTENA_Msk (1UL << 0)

extern uint32_t SystemCoreClock;
extern bsp_stub_coredebug_t *const CoreDebug;
extern bsp_stub_dwt_t *const DWT;
extern bsp_stub_scb_t *const SCB;

uint32_t __get_PRIMASK(void);
void __set_PRIMASK(uint32_t primask);
void __disable_irq(void);
uint32_t __get_MSP(void);
uint32_t __get_PSP(void);
uint32_t HAL_GetTick(void);

typedef void (*bsp_stub_gpio_write_fn_t)(GPIO_TypeDef *port, uint16_t pin, GPIO_PinState state);
typedef HAL_StatusTypeDef (*bsp_stub_i2c_probe_fn_t)(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                                     uint32_t trials, uint32_t timeout_ms);
typedef HAL_StatusTypeDef (*bsp_stub_i2c_mem_fn_t)(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                                                   uint16_t mem_addr, uint16_t mem_add_size,
                                                   uint8_t *data, uint16_t size,
                                                   uint32_t timeout_ms);

void BSP_Stub_ResetAll(void);
void BSP_Stub_SetTickMs(uint32_t tick_ms);
void BSP_Stub_SetCoreRegs(uint32_t msp, uint32_t psp);
void BSP_Stub_SetEncoderStartStatus(HAL_StatusTypeDef st);
void BSP_Stub_SetPwmStatus(HAL_StatusTypeDef st);
void BSP_Stub_SetUartTxStatus(HAL_StatusTypeDef st);
void BSP_Stub_SetUartRxStatus(HAL_StatusTypeDef st);
void BSP_Stub_SetGpioWriteHook(bsp_stub_gpio_write_fn_t fn);
void BSP_Stub_I2C_SetHandlers(bsp_stub_i2c_probe_fn_t probe_fn, bsp_stub_i2c_mem_fn_t write_fn,
                              bsp_stub_i2c_mem_fn_t read_fn);

void BSP_Stub_FlashConfig(uint32_t base_addr, uint32_t total_size_bytes, uint32_t erase_unit_bytes);
uint8_t *BSP_Stub_FlashBuffer(void);
void BSP_Stub_FlashSetReadStatus(uint32_t status_val);
void BSP_Stub_FlashSetWriteStatus(uint32_t status_val);
void BSP_Stub_FlashSetEraseStatus(uint32_t status_val);
void BSP_Stub_FlashSetCorruptNextWrite(bool corrupt);

#endif /* LAYERED_CAR_BSP_HOST_TYPES_H */
