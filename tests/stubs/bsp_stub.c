/**
 * @file bsp_stub.c
 * @brief 宿主机单元测试 STM32 HAL、Cortex-M 核心与内存模拟 Flash 桩实现。
 * @ingroup tests
 */

#include "bsp_flash.h"
#include "bsp_mcu.h"
#include "rtos_stub.h"
#include <string.h>

#define BSP_STUB_FLASH_MAX_BYTES 65536U

static bsp_stub_coredebug_t s_coredebug;
static bsp_stub_dwt_t s_dwt;
static bsp_stub_scb_t s_scb;

uint32_t SystemCoreClock = 168000000U;
bsp_stub_coredebug_t *const CoreDebug = &s_coredebug;
bsp_stub_dwt_t *const DWT = &s_dwt;
bsp_stub_scb_t *const SCB = &s_scb;

static uint32_t s_primask;
static uint32_t s_msp = 0x20020000U;
static uint32_t s_psp = 0x20010000U;
static uint32_t s_tick_ms;

static HAL_StatusTypeDef s_enc_start_status = HAL_OK;
static HAL_StatusTypeDef s_pwm_status = HAL_OK;
static HAL_StatusTypeDef s_uart_tx_status = HAL_OK;
static HAL_StatusTypeDef s_uart_rx_status = HAL_OK;

static bsp_stub_gpio_write_fn_t s_gpio_write_hook;
static bsp_stub_i2c_probe_fn_t s_i2c_probe_fn;
static bsp_stub_i2c_mem_fn_t s_i2c_write_fn;
static bsp_stub_i2c_mem_fn_t s_i2c_read_fn;

static uint8_t s_flash_mem[BSP_STUB_FLASH_MAX_BYTES];
static uint32_t s_flash_base = 0x1000U;
static uint32_t s_flash_total_bytes = 0x1000U;
static uint32_t s_flash_erase_unit = 0x100U;
static bsp_status_t s_flash_read_status = BSP_OK;
static bsp_status_t s_flash_write_status = BSP_OK;
static bsp_status_t s_flash_erase_status = BSP_OK;
static bool s_flash_corrupt_next_write;

void BSP_Stub_ResetAll(void) {
  (void)memset(&s_coredebug, 0, sizeof(s_coredebug));
  (void)memset(&s_dwt, 0, sizeof(s_dwt));
  (void)memset(&s_scb, 0, sizeof(s_scb));
  SystemCoreClock = 168000000U;
  s_primask = 0U;
  s_msp = 0x20020000U;
  s_psp = 0x20010000U;
  s_tick_ms = 0U;

  s_enc_start_status = HAL_OK;
  s_pwm_status = HAL_OK;
  s_uart_tx_status = HAL_OK;
  s_uart_rx_status = HAL_OK;

  s_gpio_write_hook = NULL;
  s_i2c_probe_fn = NULL;
  s_i2c_write_fn = NULL;
  s_i2c_read_fn = NULL;

  (void)memset(s_flash_mem, 0xFF, sizeof(s_flash_mem));
  s_flash_base = 0x1000U;
  s_flash_total_bytes = 0x1000U;
  s_flash_erase_unit = 0x100U;
  s_flash_read_status = BSP_OK;
  s_flash_write_status = BSP_OK;
  s_flash_erase_status = BSP_OK;
  s_flash_corrupt_next_write = false;
  RTOS_Stub_Reset();
}

static uint32_t s_rtos_tick_count;
static uint32_t s_rtos_tick_freq_hz = 1000U;
static uint32_t s_rtos_stack_free_bytes;
static bool s_rtos_thread_new_fail;
static osStatus_t s_rtos_delay_until_status = osOK;
static rtos_stub_delay_until_fn_t s_rtos_delay_until_fn;
static uint32_t s_rtos_max_loop_iters;
static uint32_t s_rtos_loop_iters;
static osThreadFunc_t s_rtos_last_thread_func;
static void *s_rtos_last_thread_arg;
static osThreadAttr_t s_rtos_last_thread_attr;
static bool s_rtos_has_thread_attr;
static osThreadId_t s_rtos_current_thread_id;
static uint32_t s_rtos_last_delay_until_tick;
static uint32_t s_rtos_stack_space_calls;
static uint32_t s_rtos_dummy_handle;

void RTOS_Stub_Reset(void) {
  s_rtos_tick_count = 0U;
  s_rtos_tick_freq_hz = 1000U;
  s_rtos_stack_free_bytes = 0U;
  s_rtos_thread_new_fail = false;
  s_rtos_delay_until_status = osOK;
  s_rtos_delay_until_fn = NULL;
  s_rtos_max_loop_iters = 0U;
  s_rtos_loop_iters = 0U;
  s_rtos_last_thread_func = NULL;
  s_rtos_last_thread_arg = NULL;
  (void)memset(&s_rtos_last_thread_attr, 0, sizeof(s_rtos_last_thread_attr));
  s_rtos_has_thread_attr = false;
  s_rtos_current_thread_id = NULL;
  s_rtos_last_delay_until_tick = 0U;
  s_rtos_stack_space_calls = 0U;
}

void RTOS_Stub_SetTickCount(uint32_t tick) { s_rtos_tick_count = tick; }

void RTOS_Stub_SetTickFreq(uint32_t freq_hz) { s_rtos_tick_freq_hz = freq_hz; }

void RTOS_Stub_SetStackSpace(uint32_t free_bytes) { s_rtos_stack_free_bytes = free_bytes; }

void RTOS_Stub_SetThreadNewFail(bool fail) { s_rtos_thread_new_fail = fail; }

void RTOS_Stub_SetDelayUntilStatus(osStatus_t status) { s_rtos_delay_until_status = status; }

void RTOS_Stub_SetDelayUntilHandler(rtos_stub_delay_until_fn_t fn) { s_rtos_delay_until_fn = fn; }

void RTOS_Stub_SetMaxLoopIterations(uint32_t max_iters) {
  s_rtos_max_loop_iters = max_iters;
  s_rtos_loop_iters = 0U;
}

bool RTOS_Stub_ShouldContinueLoop(void) {
  if (s_rtos_loop_iters >= s_rtos_max_loop_iters) {
    return false;
  }
  s_rtos_loop_iters++;
  return true;
}

osThreadFunc_t RTOS_Stub_GetLastThreadFunc(void) { return s_rtos_last_thread_func; }

void *RTOS_Stub_GetLastThreadArg(void) { return s_rtos_last_thread_arg; }

const osThreadAttr_t *RTOS_Stub_GetLastThreadAttr(void) {
  return s_rtos_has_thread_attr ? &s_rtos_last_thread_attr : NULL;
}

uint32_t RTOS_Stub_GetLastDelayUntilTick(void) { return s_rtos_last_delay_until_tick; }

uint32_t RTOS_Stub_GetStackSpaceCalls(void) { return s_rtos_stack_space_calls; }

osKernelState_t osKernelGetState(void) { return osKernelInactive; }

osStatus_t osDelay(uint32_t ticks) {
  (void)ticks;
  return osOK;
}

osStatus_t osDelayUntil(uint32_t ticks) {
  s_rtos_last_delay_until_tick = ticks;
  if (s_rtos_delay_until_fn != NULL) {
    return s_rtos_delay_until_fn(ticks);
  }
  if (s_rtos_delay_until_status == osOK) {
    s_rtos_tick_count = ticks;
  }
  return s_rtos_delay_until_status;
}

uint32_t osKernelGetTickCount(void) { return s_rtos_tick_count; }

uint32_t osKernelGetTickFreq(void) { return s_rtos_tick_freq_hz; }

osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr) {
  s_rtos_last_thread_func = func;
  s_rtos_last_thread_arg = argument;
  if (attr != NULL) {
    s_rtos_last_thread_attr = *attr;
    s_rtos_has_thread_attr = true;
  } else {
    (void)memset(&s_rtos_last_thread_attr, 0, sizeof(s_rtos_last_thread_attr));
    s_rtos_has_thread_attr = false;
  }
  if (s_rtos_thread_new_fail || func == NULL) {
    return NULL;
  }
  osThreadId_t handle = (attr != NULL && attr->cb_mem != NULL) ? (osThreadId_t)attr->cb_mem
                                                               : (osThreadId_t)&s_rtos_dummy_handle;
  s_rtos_current_thread_id = handle;
  return handle;
}

osThreadId_t osThreadGetId(void) { return s_rtos_current_thread_id; }

uint32_t osThreadGetStackSpace(osThreadId_t thread_id) {
  (void)thread_id;
  s_rtos_stack_space_calls++;
  return s_rtos_stack_free_bytes;
}

uint32_t xTaskGetSchedulerState(void) { return taskSCHEDULER_NOT_STARTED; }

void BSP_Stub_SetTickMs(uint32_t tick_ms) { s_tick_ms = tick_ms; }

void BSP_Stub_SetCoreRegs(uint32_t msp, uint32_t psp) {
  s_msp = msp;
  s_psp = psp;
}

void BSP_Stub_SetEncoderStartStatus(HAL_StatusTypeDef st) { s_enc_start_status = st; }

void BSP_Stub_SetPwmStatus(HAL_StatusTypeDef st) { s_pwm_status = st; }

void BSP_Stub_SetUartTxStatus(HAL_StatusTypeDef st) { s_uart_tx_status = st; }

void BSP_Stub_SetUartRxStatus(HAL_StatusTypeDef st) { s_uart_rx_status = st; }

void BSP_Stub_SetGpioWriteHook(bsp_stub_gpio_write_fn_t fn) { s_gpio_write_hook = fn; }

void BSP_Stub_I2C_SetHandlers(bsp_stub_i2c_probe_fn_t probe_fn, bsp_stub_i2c_mem_fn_t write_fn,
                              bsp_stub_i2c_mem_fn_t read_fn) {
  s_i2c_probe_fn = probe_fn;
  s_i2c_write_fn = write_fn;
  s_i2c_read_fn = read_fn;
}

void BSP_Stub_FlashConfig(uint32_t base_addr, uint32_t total_size_bytes,
                          uint32_t erase_unit_bytes) {
  s_flash_base = base_addr;
  s_flash_total_bytes =
      (total_size_bytes <= BSP_STUB_FLASH_MAX_BYTES) ? total_size_bytes : BSP_STUB_FLASH_MAX_BYTES;
  s_flash_erase_unit = erase_unit_bytes;
  (void)memset(s_flash_mem, 0xFF, s_flash_total_bytes);
}

uint8_t *BSP_Stub_FlashBuffer(void) { return s_flash_mem; }

void BSP_Stub_FlashSetReadStatus(uint32_t status_val) {
  s_flash_read_status = (bsp_status_t)status_val;
}

void BSP_Stub_FlashSetWriteStatus(uint32_t status_val) {
  s_flash_write_status = (bsp_status_t)status_val;
}

void BSP_Stub_FlashSetEraseStatus(uint32_t status_val) {
  s_flash_erase_status = (bsp_status_t)status_val;
}

void BSP_Stub_FlashSetCorruptNextWrite(bool corrupt) { s_flash_corrupt_next_write = corrupt; }

uint32_t __get_PRIMASK(void) { return s_primask; }

void __set_PRIMASK(uint32_t primask) { s_primask = primask; }

void __disable_irq(void) { s_primask = 1U; }

uint32_t __get_MSP(void) { return s_msp; }

uint32_t __get_PSP(void) { return s_psp; }

uint32_t HAL_GetTick(void) { return s_tick_ms; }

void HAL_GPIO_WritePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState) {
  if (GPIOx == NULL) {
    return;
  }
  if (s_gpio_write_hook != NULL) {
    s_gpio_write_hook(GPIOx, GPIO_Pin, PinState);
  }
  if (PinState == GPIO_PIN_SET) {
    GPIOx->odr |= GPIO_Pin;
    GPIOx->idr |= GPIO_Pin;
  } else {
    GPIOx->odr &= ~(uint32_t)GPIO_Pin;
    GPIOx->idr &= ~(uint32_t)GPIO_Pin;
  }
}

GPIO_PinState HAL_GPIO_ReadPin(const GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
  if (GPIOx == NULL) {
    return GPIO_PIN_RESET;
  }
  return ((GPIOx->idr & GPIO_Pin) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

void HAL_GPIO_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin) {
  if (GPIOx == NULL) {
    return;
  }
  GPIOx->odr ^= GPIO_Pin;
  GPIOx->idr ^= GPIO_Pin;
}

HAL_StatusTypeDef HAL_TIM_PWM_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
  if (s_pwm_status != HAL_OK) {
    return s_pwm_status;
  }
  if (htim != NULL) {
    htim->started_pwm_mask |= (1U << (Channel >> 2U));
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_PWM_Stop(TIM_HandleTypeDef *htim, uint32_t Channel) {
  if (s_pwm_status != HAL_OK) {
    return s_pwm_status;
  }
  if (htim != NULL) {
    htim->started_pwm_mask &= ~(1U << (Channel >> 2U));
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Encoder_Start(TIM_HandleTypeDef *htim, uint32_t Channel) {
  (void)Channel;
  if (s_enc_start_status != HAL_OK) {
    return s_enc_start_status;
  }
  if (htim != NULL) {
    htim->started_encoder = true;
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_TIM_Encoder_Stop(TIM_HandleTypeDef *htim, uint32_t Channel) {
  (void)Channel;
  if (htim != NULL) {
    htim->started_encoder = false;
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, const uint8_t *pData, uint16_t Size,
                                    uint32_t Timeout) {
  (void)Timeout;
  if (huart == NULL || pData == NULL || Size == 0U) {
    return HAL_ERROR;
  }
  if (s_uart_tx_status != HAL_OK) {
    return s_uart_tx_status;
  }
  huart->last_tx_data = pData;
  huart->last_tx_len = Size;
  uint16_t copy_len =
      (Size <= sizeof(huart->last_tx_copy)) ? Size : (uint16_t)sizeof(huart->last_tx_copy);
  (void)memcpy(huart->last_tx_copy, pData, copy_len);
  if ((uint32_t)huart->tx_history_len + (uint32_t)Size <= sizeof(huart->tx_history)) {
    (void)memcpy(&huart->tx_history[huart->tx_history_len], pData, Size);
    huart->tx_history_len = (uint16_t)(huart->tx_history_len + Size);
  }
  huart->tx_blocking_calls++;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Transmit_DMA(UART_HandleTypeDef *huart, const uint8_t *pData,
                                        uint16_t Size) {
  if (huart == NULL || pData == NULL || Size == 0U) {
    return HAL_ERROR;
  }
  if (s_uart_tx_status != HAL_OK) {
    return s_uart_tx_status;
  }
  huart->gState = HAL_UART_STATE_BUSY_TX;
  huart->last_tx_data = pData;
  huart->last_tx_len = Size;
  huart->tx_dma_calls++;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *huart, uint8_t *pData, uint16_t Size) {
  if (huart == NULL || pData == NULL || Size == 0U) {
    return HAL_ERROR;
  }
  if (s_uart_rx_status != HAL_OK) {
    return s_uart_rx_status;
  }
  huart->rx_buf = pData;
  huart->rx_size = Size;
  huart->rx_it_calls++;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart, uint8_t *pData,
                                               uint16_t Size) {
  if (huart == NULL || pData == NULL || Size == 0U) {
    return HAL_ERROR;
  }
  if (s_uart_rx_status != HAL_OK) {
    return s_uart_rx_status;
  }
  huart->rx_buf = pData;
  huart->rx_size = Size;
  huart->rx_dma_idle_calls++;
  return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                        uint32_t Trials, uint32_t Timeout) {
  if (s_i2c_probe_fn != NULL) {
    return s_i2c_probe_fn(hi2c, DevAddress, Trials, Timeout);
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Write(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                    uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                    uint16_t Size, uint32_t Timeout) {
  if (s_i2c_write_fn != NULL) {
    return s_i2c_write_fn(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, Timeout);
  }
  return HAL_OK;
}

HAL_StatusTypeDef HAL_I2C_Mem_Read(I2C_HandleTypeDef *hi2c, uint16_t DevAddress,
                                   uint16_t MemAddress, uint16_t MemAddSize, uint8_t *pData,
                                   uint16_t Size, uint32_t Timeout) {
  if (s_i2c_read_fn != NULL) {
    return s_i2c_read_fn(hi2c, DevAddress, MemAddress, MemAddSize, pData, Size, Timeout);
  }
  return HAL_OK;
}

static bool bsp_stub_flash_range_valid(uint32_t addr, uint32_t size_bytes) {
  if (size_bytes == 0U || s_flash_total_bytes == 0U || addr < s_flash_base) {
    return false;
  }
  uint32_t offset = addr - s_flash_base;
  if (offset >= s_flash_total_bytes) {
    return false;
  }
  return size_bytes <= (s_flash_total_bytes - offset);
}

uint32_t BSP_Flash_GetEraseUnitSize(uint32_t addr) {
  return bsp_stub_flash_range_valid(addr, 1U) ? s_flash_erase_unit : 0U;
}

bsp_status_t BSP_Flash_Read(uint32_t addr, void *out, uint32_t size_bytes) {
  if (out == NULL || !bsp_stub_flash_range_valid(addr, size_bytes)) {
    return BSP_ERR_INVALID_ARG;
  }
  if (s_flash_read_status != BSP_OK) {
    return s_flash_read_status;
  }
  (void)memcpy(out, &s_flash_mem[addr - s_flash_base], size_bytes);
  return BSP_OK;
}

bsp_status_t BSP_Flash_Write(uint32_t addr, const void *data, uint32_t size_bytes) {
  if (data == NULL || !bsp_stub_flash_range_valid(addr, size_bytes) || ((addr & 3U) != 0U) ||
      ((size_bytes & 3U) != 0U)) {
    return BSP_ERR_INVALID_ARG;
  }
  if (s_flash_write_status != BSP_OK) {
    return s_flash_write_status;
  }
  uint32_t offset = addr - s_flash_base;
  (void)memcpy(&s_flash_mem[offset], data, size_bytes);
  if (s_flash_corrupt_next_write) {
    s_flash_corrupt_next_write = false;
    s_flash_mem[offset] ^= 0x5AU;
  }
  return BSP_OK;
}

bsp_status_t BSP_Flash_Erase(uint32_t start_addr, uint32_t size_bytes) {
  if (!bsp_stub_flash_range_valid(start_addr, size_bytes) || s_flash_erase_unit == 0U ||
      (start_addr % s_flash_erase_unit) != 0U) {
    return BSP_ERR_INVALID_ARG;
  }
  if (s_flash_erase_status != BSP_OK) {
    return s_flash_erase_status;
  }
  (void)memset(&s_flash_mem[start_addr - s_flash_base], 0xFF, size_bytes);
  return BSP_OK;
}
