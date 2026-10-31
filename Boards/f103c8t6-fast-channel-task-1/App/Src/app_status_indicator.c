/**
 * @file app_status_indicator.c
 * @brief 快速通道任务一蜂鸣器和 LED 提示实现。
 * @ingroup app
 */

#include "app_status_indicator.h"
#include "app_fast_channel_config.h"
#include "system_time_window.h"
#include <assert.h>

/**
 * @brief 根据布尔值开关蜂鸣器。
 * @param indicator_h 非空指示器。
 * @param enabled true 开启，false 关闭。
 * @pre indicator_h 已初始化。
 * @post GPIO 逻辑状态与 enabled 一致。
 * @note 内部辅助函数，不阻塞、不分配内存。
 */
static void app_statusindicator_writebuzzer(
  app_status_indicator_t *indicator_h,
  bool enabled
) {
  assert(indicator_h);
  if (enabled) {
    Dev_LED_Base_TurnOn(&indicator_h->buzzer_h.led_base);
  } else {
    Dev_LED_Base_TurnOff(&indicator_h->buzzer_h.led_base);
  }
}

/**
 * @brief 启动双短鸣。
 * @param indicator_h 非空指示器。
 * @param now_ms 相位起点，单位 ms。
 * @pre indicator_h 已初始化。
 * @post 蜂鸣器立即开启，相位置为第一段短鸣。
 * @note 内部辅助函数，不阻塞、不分配内存。
 */
static void app_statusindicator_startshort(
  app_status_indicator_t *indicator_h,
  uint32_t now_ms
) {
  indicator_h->buzzer_phase = APP_BUZZER_PHASE_SHORT_ON_1;
  indicator_h->phase_started_ms = now_ms;
  app_statusindicator_writebuzzer(indicator_h, true);
}

/**
 * @brief 启动长鸣。
 * @param indicator_h 非空指示器。
 * @param now_ms 相位起点，单位 ms。
 * @pre indicator_h 已初始化。
 * @post 蜂鸣器立即开启，相位置为长鸣。
 * @note 内部辅助函数，不阻塞、不分配内存。
 */
static void app_statusindicator_startlong(
  app_status_indicator_t *indicator_h,
  uint32_t now_ms
) {
  indicator_h->buzzer_phase = APP_BUZZER_PHASE_LONG_ON;
  indicator_h->phase_started_ms = now_ms;
  app_statusindicator_writebuzzer(indicator_h, true);
}

/**
 * @brief 初始化蜂鸣器和 LED 输出为关闭状态。
 * @param indicator_h 待初始化指示器，不得为 NULL。
 * @pre GPIO 和 Platform 已由 CubeMX 初始化，且只在任务启动前调用。
 * @post 两个 GPIO 均输出无效电平，蜂鸣器和 LED 状态为关闭。
 * @note 不阻塞、不分配内存。
 */
void App_StatusIndicator_Init(app_status_indicator_t *indicator_h) {
  assert(indicator_h);

  platform_gpio_pin_state_t buzzer_active =
    APP_BUZZER_ON_LEVEL != 0U
      ? PLATFORM_GPIO_PIN_SET
      : PLATFORM_GPIO_PIN_RESET;
  platform_gpio_pin_state_t led_active =
    APP_LED_ON_LEVEL != 0U
      ? PLATFORM_GPIO_PIN_SET
      : PLATFORM_GPIO_PIN_RESET;

  Dev_LED_GPIO_Init(
    &indicator_h->buzzer_h,
    "buzzer",
    APP_BUZZER_PORT,
    APP_BUZZER_PIN,
    buzzer_active
  );
  Dev_LED_GPIO_Init(
    &indicator_h->led_h,
    "led",
    APP_LED_PORT,
    APP_LED_PIN,
    led_active
  );
  indicator_h->buzzer_phase = APP_BUZZER_PHASE_OFF;
  indicator_h->phase_started_ms = 0u;
  indicator_h->last_led_toggle_ms = 0u;
  indicator_h->led_output_on = false;
  indicator_h->initialized = true;
}

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
) {
  assert(indicator_h);
  if (!indicator_h->initialized) {
    return;
  }

  if ((events & APP_CALIBRATION_EVENT_FACE_SAVED) != 0u) {
    app_statusindicator_startshort(indicator_h, now_ms);
  }
  if ((events & APP_CALIBRATION_EVENT_MAIN_ENTERED) != 0u) {
    app_statusindicator_startlong(indicator_h, now_ms);
  }
}

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
) {
  assert(indicator_h);
  if (!indicator_h->initialized) {
    return;
  }

  switch (indicator_h->buzzer_phase) {
    case APP_BUZZER_PHASE_SHORT_ON_1:
      if (Sys_Time_HasElapsedMs(
            now_ms,
            indicator_h->phase_started_ms,
            APP_BUZZER_SHORT_ON_MS
          )) {
        app_statusindicator_writebuzzer(indicator_h, false);
        indicator_h->buzzer_phase = APP_BUZZER_PHASE_SHORT_GAP;
        indicator_h->phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_SHORT_GAP:
      if (Sys_Time_HasElapsedMs(
            now_ms,
            indicator_h->phase_started_ms,
            APP_BUZZER_SHORT_GAP_MS
          )) {
        app_statusindicator_writebuzzer(indicator_h, true);
        indicator_h->buzzer_phase = APP_BUZZER_PHASE_SHORT_ON_2;
        indicator_h->phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_SHORT_ON_2:
      if (Sys_Time_HasElapsedMs(
            now_ms,
            indicator_h->phase_started_ms,
            APP_BUZZER_SHORT_ON_MS
          )) {
        app_statusindicator_writebuzzer(indicator_h, false);
        indicator_h->buzzer_phase = APP_BUZZER_PHASE_OFF;
        indicator_h->phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_LONG_ON:
      if (Sys_Time_HasElapsedMs(
            now_ms,
            indicator_h->phase_started_ms,
            APP_BUZZER_LONG_ON_MS
          )) {
        app_statusindicator_writebuzzer(indicator_h, false);
        indicator_h->buzzer_phase = APP_BUZZER_PHASE_OFF;
        indicator_h->phase_started_ms = now_ms;
      }
      break;

    case APP_BUZZER_PHASE_OFF:
    default:
      app_statusindicator_writebuzzer(indicator_h, false);
      break;
  }

  uint32_t half_period_ms =
    state == APP_FAST_CHANNEL_STATE_MAIN ? 500u : 250u;
  if (Sys_Time_HasElapsedMs(
        now_ms,
        indicator_h->last_led_toggle_ms,
        half_period_ms
      )) {
    indicator_h->last_led_toggle_ms = now_ms;
    indicator_h->led_output_on = !indicator_h->led_output_on;
    if (indicator_h->led_output_on) {
      Dev_LED_Base_TurnOn(&indicator_h->led_h.led_base);
    } else {
      Dev_LED_Base_TurnOff(&indicator_h->led_h.led_base);
    }
  }
}
