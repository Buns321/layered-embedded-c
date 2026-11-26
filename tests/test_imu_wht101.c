/**
 * @file test_imu_wht101.c
 * @brief imu_wht101 帧同步、错误重同步与上电自动归零状态机单元测试。
 * @ingroup tests
 */

#include "filter_passthrough.h"
#include "imu_wht101.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static void push_wht_frame(UART_HandleTypeDef *huart, uint8_t type, int16_t raw_val) {
  uint8_t frame[11] = {0x55U,
                       type,
                       0U,
                       0U,
                       0U,
                       0U,
                       (uint8_t)(raw_val & 0xFF),
                       (uint8_t)(((uint16_t)raw_val >> 8U) & 0xFFU),
                       0U,
                       0U,
                       0U};
  uint8_t sum = 0U;
  for (uint8_t i = 0U; i < 10U; ++i) {
    sum = (uint8_t)(sum + frame[i]);
  }
  frame[10] = sum;
  for (uint8_t i = 0U; i < 11U; ++i) {
    *huart->rx_buf = frame[i];
    HAL_UART_RxCpltCallback(huart);
  }
}

static void test_parser_resync_and_manual_mode(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();

  UART_HandleTypeDef huart = {0};
  filter_passthrough_t pt;
  assert(Filter_Passthrough_Init(&pt, "pt") == FILTER_STATUS_OK);

  imu_wht101_t imu;
  assert(Dev_IMU_WHT101_Init(&imu, &(imu_wht101_cfg_t){
                                       .name = "wht",
                                       .huart = &huart,
                                       .filter = &pt.base,
                                       .auto_zero_on_boot = false,
                                   }) == BSP_OK);
  Dev_IMU_Base_Start(&imu.base);
  assert(Dev_IMU_WHT101_IsReady(&imu));

  /* 先消费半帧使解析器停在 RECEIVING_DATA，再由串口错误触发重同步并接收完整帧。 */
  *huart.rx_buf = 0x55U;
  HAL_UART_RxCpltCallback(&huart);
  *huart.rx_buf = 0x53U;
  HAL_UART_RxCpltCallback(&huart);
  Dev_IMU_Base_Process(&imu.base, 50U);
  HAL_UART_ErrorCallback(&huart);

  push_wht_frame(&huart, 0x53U, 16384);  /* +90 deg */
  push_wht_frame(&huart, 0x52U, -16384); /* -1000 dps */
  Dev_IMU_Base_Process(&imu.base, 100U);

  assert(fabsf(Dev_IMU_Base_GetYaw(&imu.base) - 90.0f) < 0.02f);
  assert(fabsf(Dev_IMU_Base_GetGyroZ(&imu.base) + 1000.0f) < 0.1f);
}

static void test_auto_zero_boot_state_machine(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();

  UART_HandleTypeDef huart = {0};
  filter_passthrough_t pt;
  assert(Filter_Passthrough_Init(&pt, "pt") == FILTER_STATUS_OK);

  imu_wht101_t imu;
  assert(Dev_IMU_WHT101_Init(&imu, &(imu_wht101_cfg_t){
                                       .name = "wht_boot",
                                       .huart = &huart,
                                       .filter = &pt.base,
                                       .auto_zero_on_boot = true,
                                       .boot_rate = IMU_WHT101_RATE_10HZ,
                                   }) == BSP_OK);
  Dev_IMU_Base_Start(&imu.base);
  assert(!Dev_IMU_WHT101_IsReady(&imu));

  /* 1. 速率配置状态机：t=0(KEY_UNLOCK) -> t=200(RRATE) -> t=300(SAVE) -> WAIT_STILL。 */
  Dev_IMU_Base_Process(&imu.base, 0U);
  Dev_IMU_Base_Process(&imu.base, 200U);
  Dev_IMU_Base_Process(&imu.base, 300U);
  assert(Dev_IMU_WHT101_GetBootState(&imu) == IMU_WHT101_BOOT_WAIT_STILL);
  assert(huart.tx_history_len == 15U);
  assert(huart.tx_history[2] == 0x69U && huart.tx_history[7] == 0x03U &&
         huart.tx_history[8] == IMU_WHT101_RATE_10HZ && huart.tx_history[12] == 0x00U);

  /* 2. 静止确认 (>= 400 ms)：喂入 0.5 deg/s 静止帧，推进至 ZERO_CONFIG。 */
  push_wht_frame(&huart, 0x52U, 8);
  push_wht_frame(&huart, 0x53U, 4096);
  Dev_IMU_Base_Process(&imu.base, 350U);
  push_wht_frame(&huart, 0x52U, 8);
  push_wht_frame(&huart, 0x53U, 4096);
  Dev_IMU_Base_Process(&imu.base, 760U);
  assert(Dev_IMU_WHT101_GetBootState(&imu) == IMU_WHT101_BOOT_ZERO_CONFIG);

  /* 3. 硬件归零命令序列：t=770(UNLOCK) -> t=970(CALIYAW) -> t=1470(SAVE)。 */
  Dev_IMU_Base_Process(&imu.base, 770U);
  Dev_IMU_Base_Process(&imu.base, 970U);
  Dev_IMU_Base_Process(&imu.base, 1470U);
  assert(Dev_IMU_WHT101_GetBootState(&imu) == IMU_WHT101_BOOT_WAIT_ZERO_FRAME);
  assert(huart.tx_history_len == 30U && huart.tx_history[22] == 0x76U);

  /* 4. 收到小残余角（~1.5 deg），自动触发软件 ZeroYaw 并就绪。 */
  push_wht_frame(&huart, 0x53U, 273);
  Dev_IMU_Base_Process(&imu.base, 1500U);
  assert(Dev_IMU_WHT101_IsReady(&imu));
  assert(fabsf(Dev_IMU_Base_GetYaw(&imu.base)) < 1e-4f);
}

static void test_boot_fault_on_tx_error(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();

  UART_HandleTypeDef huart = {0};
  filter_passthrough_t pt;
  assert(Filter_Passthrough_Init(&pt, "pt") == FILTER_STATUS_OK);
  BSP_Stub_SetUartTxStatus(HAL_ERROR);

  imu_wht101_t imu;
  assert(Dev_IMU_WHT101_Init(&imu, &(imu_wht101_cfg_t){
                                       .name = "wht_err",
                                       .huart = &huart,
                                       .filter = &pt.base,
                                       .auto_zero_on_boot = true,
                                   }) == BSP_OK);
  Dev_IMU_Base_Start(&imu.base);
  Dev_IMU_Base_Process(&imu.base, 10U);
  assert(Dev_IMU_WHT101_HasFault(&imu));
}

int main(void) {
  test_parser_resync_and_manual_mode();
  test_auto_zero_boot_state_machine();
  test_boot_fault_on_tx_error();
  (void)puts("test_imu_wht101 passed");
  return 0;
}
