/**
 * @file app_status_indicator.h
 * @brief 快速通道任务一蜂鸣器和 LED 提示。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_STATUS_INDICATOR_H
#define LAYERED_EMBEDDED_C_APP_STATUS_INDICATOR_H

#include "app_calibration_flow.h"
#include "led_gpio.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 蜂鸣器提示相位。
 */
typedef enum app_buzzer_phase_e {
  APP_BUZZER_PHASE_OFF = 0U,
  APP_BUZZER_PHASE_SHORT_ON_1,
  APP_BUZZER_PHASE_SHORT_GAP,
  APP_BUZZER_PHASE_SHORT_ON_2,
  APP_BUZZER_PHASE_LONG_ON,
} app_buzzer_phase_t;

/**
 * @brief 状态指示器对象。
 */
typedef struct app_status_indicator_s {
  led_gpio_t buzzer_h;
  led_gpio_t led_h;
  app_buzzer_phase_t buzzer_phase;
  uint32_t phase_started_ms;
  uint32_t last_led_toggle_ms;
  bool led_output_on;
  bool initialized;
} app_status_indicator_t;

/**
 * @brief 初始化蜂鸣器和 LED 输出为关闭状态。
 * @param indicator_h 待初始化指示器，不得为 NULL。
 * @pre GPIO 和 Platform 已由 CubeMX 初始化，且只在任务启动前调用。
 * @post 两个 GPIO 均输出无效电平，蜂鸣器和 LED 状态为关闭。
 * @note 不阻塞、不分配内存。
 */
void App_StatusIndicator_Init(app_status_indicator_t *indicator_h);

/**
 * @brief 根据标定事件启动蜂鸣器提示。
 * @param indicator_h 已初始化指示器，不得为 NULL。
 * @param events 标定流程事件位组合。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 单任务调用。
 * @post FACE_SAVED 启动双短鸣，MAIN_ENTERED 启动长鸣；同时存在时后者覆盖前者。
 * @note 不阻塞、不分配内存。
 */
void App_StatusIndicator_Notify(
  app_status_indicator_t *indicator_h,
  uint32_t events,
  uint32_t now_ms
);

/**
 * @brief 推进蜂鸣器相位和 LED 闪烁。
 * @param indicator_h 已初始化指示器，不得为 NULL。
 * @param state 当前标定状态。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 单任务调用。
 * @post 到期蜂鸣器相位最多推进一次；主运行发光周期为 1000 ms，其他状态为 500 ms。
 * @note 不阻塞、不分配内存。
 */
void App_StatusIndicator_Process(
  app_status_indicator_t *indicator_h,
  app_fast_channel_state_t state,
  uint32_t now_ms
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_STATUS_INDICATOR_H
