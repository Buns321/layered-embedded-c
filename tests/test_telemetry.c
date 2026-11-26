/**
 * @file test_telemetry.c
 * @brief telemetry_base / JustFloat / FireWater / DMA 丢帧与异常统计测试。
 * @ingroup tests
 */

#include "telem_firewater.h"
#include "telem_justfloat.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void test_justfloat_and_dma_states(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();

  UART_HandleTypeDef huart = {.gState = HAL_UART_STATE_READY};
  telem_justfloat_t jf;
  assert(Dev_Telem_JustFloat_Init(&jf, &(telem_justfloat_cfg_t){
                                           .name = "jf",
                                           .huart = &huart,
                                           .channel_count = 3U,
                                       }) == BSP_OK);

  /* 非法掩码与 NaN 被基类拦截。 */
  telemetry_sample_t bad = {.valid_mask = 1UL << TELEMETRY_CHANNEL_CAPACITY};
  assert(Dev_Telemetry_Base_Send(&jf.base, &bad) == DEV_TELEMETRY_ERROR);
  bad = (telemetry_sample_t){.valid_mask = 0x1U, .value = {NAN}};
  assert(Dev_Telemetry_Base_Send(&jf.base, &bad) == DEV_TELEMETRY_ERROR);

  /* ch0=1.5f, ch1=无效(写 NAN), ch2=-2.0f -> 16 字节载荷+尾帧。 */
  const telemetry_sample_t s1 = {.valid_mask = 0x5U, .value = {1.5f, 9.0f, -2.0f}, .time_ms = 10U};
  assert(Dev_Telemetry_Base_Send(&jf.base, &s1) == DEV_TELEMETRY_OK);
  assert(huart.last_tx_len == 16U);
  float f0 = 0.0f;
  float f1 = 0.0f;
  float f2 = 0.0f;
  (void)memcpy(&f0, &huart.last_tx_data[0], 4U);
  (void)memcpy(&f1, &huart.last_tx_data[4], 4U);
  (void)memcpy(&f2, &huart.last_tx_data[8], 4U);
  assert(f0 == 1.5f && isnan(f1) && f2 == -2.0f);
  const uint8_t tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};
  assert(memcmp(&huart.last_tx_data[12], tail, 4U) == 0);

  /* DMA 忙期间丢弃并递增 dropped_frames。 */
  assert(Dev_Telemetry_Base_Send(&jf.base, &s1) == DEV_TELEMETRY_BUSY);
  telemetry_status_t st = {0};
  Dev_Telemetry_Base_GetStatus(&jf.base, &st);
  assert(st.is_busy && st.dropped_frames == 1U && st.sent_frames == 0U);

  /* TX 完成恢复并允许下一帧；ErrorCallback 计数 error_count。 */
  huart.gState = HAL_UART_STATE_READY;
  HAL_UART_TxCpltCallback(&huart);
  Dev_Telemetry_Base_GetStatus(&jf.base, &st);
  assert(!st.is_busy && st.sent_frames == 1U);

  assert(Dev_Telemetry_Base_Send(&jf.base, &s1) == DEV_TELEMETRY_OK);
  huart.gState = HAL_UART_STATE_READY;
  HAL_UART_ErrorCallback(&huart);
  Dev_Telemetry_Base_GetStatus(&jf.base, &st);
  assert(!st.is_busy && st.error_count == 1U);
}

static void test_firewater_formatting(void) {
  BSP_Stub_ResetAll();
  BSP_UART_ResetSlots();

  UART_HandleTypeDef huart = {.gState = HAL_UART_STATE_READY};
  static const char *const names[2] = {"vx", "wz"};
  telem_firewater_t fw;
  assert(Dev_Telem_FireWater_Init(&fw, &(telem_firewater_cfg_t){
                                           .name = "fw",
                                           .huart = &huart,
                                           .channel_names = names,
                                           .channel_count = 2U,
                                           .precision = 3U,
                                       }) == BSP_OK);

  const telemetry_sample_t s = {.valid_mask = 0x3U, .value = {-1.25f, 0.5f}, .time_ms = 20U};
  assert(Dev_Telemetry_Base_Send(&fw.base, &s) == DEV_TELEMETRY_OK);
  const char expected[] = "vx:-1.250,wz:0.500\n";
  assert(huart.last_tx_len == (uint16_t)(sizeof(expected) - 1U));
  assert(memcmp(huart.last_tx_data, expected, huart.last_tx_len) == 0);
}

int main(void) {
  test_justfloat_and_dma_states();
  test_firewater_formatting();
  (void)puts("test_telemetry passed");
  return 0;
}
