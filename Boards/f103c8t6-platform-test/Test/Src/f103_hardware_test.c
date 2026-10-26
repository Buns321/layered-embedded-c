/**
 * @file f103_hardware_test.c
 * @brief F103C8T6 基于 Platform API 的板载外设实测与 USART2 自检命令。
 * @ingroup tests
 */

#include "f103_hardware_test.h"

#include "platform_adc.h"
#include "platform_encoder.h"
#include "platform_gpio.h"
#include "platform_i2c.h"
#include "platform_iwdg.h"
#include "platform_pwm.h"
#include "platform_uart.h"
#include "platform_wwdg.h"
#include "system_delay.h"
#include "system_log.h"

#include <string.h>

#define F103_TEST_I2C_AHT20_ADDRESS   0x38u
#define F103_TEST_I2C_OLED_ADDRESS    0x3Du
#define F103_TEST_I2C_TRIALS          2u
#define F103_TEST_I2C_TIMEOUT_MS      10u
#define F103_TEST_ADC_TIMEOUT_MS      20u
#define F103_TEST_UART_TIMEOUT_MS     100u
#define F103_TEST_UART_RING_SIZE      64u
#define F103_TEST_UART_TX_SIZE        192u
#define F103_TEST_COMMAND_SIZE        64u
#define F103_TEST_UART_DMA_TEST_SIZE  8u
#define F103_TEST_IWDG_TIMEOUT_MS     100u
#define F103_TEST_WWDG_TIMEOUT_MS     5u

#define F103_TEST_GPIO_PIN_0  0x0001u
#define F103_TEST_GPIO_PIN_5  0x0020u
#define F103_TEST_GPIO_PIN_7  0x0080u
#define F103_TEST_GPIO_PIN_9  0x0200u
#define F103_TEST_GPIO_PIN_11 0x0800u

typedef enum f103_hardware_test_result_e {
  F103_TEST_RESULT_PASS = 0,
  F103_TEST_RESULT_FAIL,
  F103_TEST_RESULT_SKIP,
} f103_hardware_test_result_t;

typedef struct f103_hardware_test_output_s {
  platform_gpio_port_t port;
  uint16_t pin;
} f103_hardware_test_output_t;

typedef struct f103_hardware_test_text_s {
  char *data;
  size_t capacity;
  size_t length;
} f103_hardware_test_text_t;

static const f103_hardware_test_output_t s_hw_outputs[] = {
  {PLATFORM_GPIO_PORT_A, F103_TEST_GPIO_PIN_7},
  {PLATFORM_GPIO_PORT_B, F103_TEST_GPIO_PIN_0},
  {PLATFORM_GPIO_PORT_B, F103_TEST_GPIO_PIN_5},
  {PLATFORM_GPIO_PORT_B, F103_TEST_GPIO_PIN_9},
  {PLATFORM_GPIO_PORT_A, F103_TEST_GPIO_PIN_11},
};

static uint8_t s_uart_rx_byte;
static volatile uint8_t s_uart_ring[F103_TEST_UART_RING_SIZE];
static volatile uint16_t s_uart_ring_head;
static volatile uint16_t s_uart_ring_tail;
static volatile bool s_uart_rearm_pending;
static volatile bool s_uart_tx_busy;
static char s_uart_tx[F103_TEST_UART_TX_SIZE];
static char s_uart_command[F103_TEST_COMMAND_SIZE];
static uint16_t s_uart_command_length;
static uint8_t s_uart_dma_test[F103_TEST_UART_DMA_TEST_SIZE];
static volatile bool s_uart_rx_dma_active;
static volatile bool s_uart_rx_dma_complete;

static uint32_t s_adc_dma[2];
static volatile bool s_adc_dma_complete;

static bool s_boot_iwdg_reset;
static bool s_boot_wwdg_reset;

static void f103_hardwaretest_append(
  f103_hardware_test_text_t *text,
  const char *value
) {
  while (
    value != NULL &&
    *value != '\0' &&
    text->length + 1u < text->capacity
  ) {
    text->data[text->length] = *value;
    value++;
    text->length++;
  }
  text->data[text->length] = '\0';
}

static void f103_hardwaretest_appenduint(
  f103_hardware_test_text_t *text,
  uint32_t value
) {
  char digits[10];
  size_t count = 0u;

  do {
    digits[count] = (char)('0' + value % 10u);
    value /= 10u;
    count++;
  } while (value != 0u && count < sizeof(digits));

  while (count > 0u) {
    count--;
    const char digit[2] = {digits[count], '\0'};
    f103_hardwaretest_append(text, digit);
  }
}

static void f103_hardwaretest_report(
  const char *name,
  f103_hardware_test_result_t result,
  f103_hardware_test_summary_t *summary
) {
  switch (result) {
    case F103_TEST_RESULT_PASS:
      summary->passed++;
      SYS_LOG_INFO("HW", "%s PASS", name);
      break;
    case F103_TEST_RESULT_SKIP:
      summary->skipped++;
      SYS_LOG_WARN("HW", "%s SKIP", name);
      break;
    default:
      summary->failed++;
      SYS_LOG_WARN("HW", "%s FAIL", name);
      break;
  }
}

static f103_hardware_test_result_t f103_hardwaretest_checkgpio(void) {
  bool passed = true;

  for (
    uint32_t i = 0u;
    i < sizeof(s_hw_outputs) / sizeof(s_hw_outputs[0]);
    i++
  ) {
    Platform_GPIO_WritePin(
      s_hw_outputs[i].port,
      s_hw_outputs[i].pin,
      PLATFORM_GPIO_PIN_RESET
    );
    if (
      Platform_GPIO_ReadPin(
        s_hw_outputs[i].port,
        s_hw_outputs[i].pin
      ) != PLATFORM_GPIO_PIN_RESET
    ) {
      passed = false;
    }

    Platform_GPIO_WritePin(
      s_hw_outputs[i].port,
      s_hw_outputs[i].pin,
      PLATFORM_GPIO_PIN_SET
    );
    if (
      Platform_GPIO_ReadPin(
        s_hw_outputs[i].port,
        s_hw_outputs[i].pin
      ) != PLATFORM_GPIO_PIN_SET
    ) {
      passed = false;
    }

    Platform_GPIO_WritePin(
      s_hw_outputs[i].port,
      s_hw_outputs[i].pin,
      PLATFORM_GPIO_PIN_RESET
    );
  }

  return passed ? F103_TEST_RESULT_PASS : F103_TEST_RESULT_FAIL;
}

static f103_hardware_test_result_t f103_hardwaretest_checki2caddress(
  uint16_t address
) {
  return Platform_I2C_Probe(
           PLATFORM_I2C_1,
           address,
           F103_TEST_I2C_TRIALS,
           F103_TEST_I2C_TIMEOUT_MS
         ) == PLATFORM_STATUS_OK
           ? F103_TEST_RESULT_PASS
           : F103_TEST_RESULT_FAIL;
}

static f103_hardware_test_result_t f103_hardwaretest_checkadc(void) {
  s_adc_dma[0] = 0u;
  s_adc_dma[1] = 0u;
  s_adc_dma_complete = false;

  platform_status_t status = Platform_ADC_StartDMA(
    PLATFORM_ADC_1,
    s_adc_dma,
    sizeof(s_adc_dma) / sizeof(s_adc_dma[0])
  );
  if (status != PLATFORM_STATUS_OK) {
    SYS_LOG_WARN(
      "HW",
      "adc start status=%lu",
      (unsigned long)status
    );
    return F103_TEST_RESULT_FAIL;
  }

  for (
    uint32_t elapsed_ms = 0u;
    !s_adc_dma_complete && elapsed_ms < F103_TEST_ADC_TIMEOUT_MS;
    elapsed_ms++
  ) {
    Sys_DelayMs(1u);
  }

  (void)Platform_ADC_StopDMA(PLATFORM_ADC_1);
  const uint16_t *samples = (const uint16_t *)s_adc_dma;
  const bool values_valid =
    samples[0] <= 0x0FFFu &&
    samples[1] <= 0x0FFFu;
  SYS_LOG_INFO(
    "HW",
    "adc raw pa4=%u pa5=%u complete=%u",
    (unsigned int)samples[0],
    (unsigned int)samples[1],
    s_adc_dma_complete ? 1u : 0u
  );

  return s_adc_dma_complete &&
         values_valid &&
         Platform_ADC_GetLastError(PLATFORM_ADC_1) == PLATFORM_ADC_ERROR_NONE
           ? F103_TEST_RESULT_PASS
           : F103_TEST_RESULT_FAIL;
}

static f103_hardware_test_result_t f103_hardwaretest_checkencoder(void) {
  platform_status_t status = Platform_Encoder_Start(PLATFORM_ENCODER_TIM1);
  if (status != PLATFORM_STATUS_OK) {
    return F103_TEST_RESULT_FAIL;
  }

  status = Platform_Encoder_SetCounter(PLATFORM_ENCODER_TIM1, 1234u);
  uint32_t counter = 0u;
  if (status == PLATFORM_STATUS_OK) {
    status = Platform_Encoder_GetCounter(
      PLATFORM_ENCODER_TIM1,
      &counter
    );
  }
  uint8_t counter_bits = 0u;
  if (status == PLATFORM_STATUS_OK) {
    status = Platform_Encoder_GetCounterBits(
      PLATFORM_ENCODER_TIM1,
      &counter_bits
    );
  }
  (void)Platform_Encoder_Stop(PLATFORM_ENCODER_TIM1);

  return status == PLATFORM_STATUS_OK &&
         counter == 1234u &&
         counter_bits == 16u &&
         Platform_Encoder_GetLastError(PLATFORM_ENCODER_TIM1) ==
           PLATFORM_ERROR_NONE
           ? F103_TEST_RESULT_PASS
           : F103_TEST_RESULT_FAIL;
}

static f103_hardware_test_result_t f103_hardwaretest_exercisepwm(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty,
  uint32_t delay_ms
) {
  platform_status_t status = Platform_PWM_SetDuty(
    pwm,
    channel,
    max_duty,
    duty
  );
  if (status == PLATFORM_STATUS_OK) {
    status = Platform_PWM_Start(pwm, channel);
  }
  if (status == PLATFORM_STATUS_OK && delay_ms != 0u) {
    Sys_DelayMs(delay_ms);
  }
  const platform_status_t stop_status = Platform_PWM_Stop(pwm, channel);
  const platform_status_t clear_status = Platform_PWM_SetCompare(
    pwm,
    channel,
    0u
  );

  return status == PLATFORM_STATUS_OK &&
         stop_status == PLATFORM_STATUS_OK &&
         clear_status == PLATFORM_STATUS_OK &&
         Platform_PWM_GetLastError(pwm) == PLATFORM_ERROR_NONE
           ? F103_TEST_RESULT_PASS
           : F103_TEST_RESULT_FAIL;
}

static f103_hardware_test_result_t f103_hardwaretest_checktim2(void) {
  const f103_hardware_test_result_t channel_1 =
    f103_hardwaretest_exercisepwm(
      PLATFORM_PWM_TIM2,
      PLATFORM_PWM_CHANNEL_1,
      3599u,
      0u,
      2u
    );
  const f103_hardware_test_result_t channel_2 =
    f103_hardwaretest_exercisepwm(
      PLATFORM_PWM_TIM2,
      PLATFORM_PWM_CHANNEL_2,
      3599u,
      0u,
      2u
    );
  return channel_1 == F103_TEST_RESULT_PASS &&
         channel_2 == F103_TEST_RESULT_PASS
           ? F103_TEST_RESULT_PASS
           : F103_TEST_RESULT_FAIL;
}

static f103_hardware_test_result_t f103_hardwaretest_checktim3(void) {
  return f103_hardwaretest_exercisepwm(
    PLATFORM_PWM_TIM3,
    PLATFORM_PWM_CHANNEL_1,
    89u,
    0u,
    2u
  );
}

static f103_hardware_test_result_t f103_hardwaretest_checktim4(void) {
  return f103_hardwaretest_exercisepwm(
    PLATFORM_PWM_TIM4,
    PLATFORM_PWM_CHANNEL_3,
    20000u,
    1500u,
    20u
  );
}

static f103_hardware_test_result_t f103_hardwaretest_checkusart3(void) {
  static const uint8_t probe[] = {'A', 'T', '\r', '\n'};
  uint8_t response[32] = {0};

  platform_status_t status = Platform_UART_Transmit(
    PLATFORM_UART_3,
    probe,
    sizeof(probe),
    F103_TEST_UART_TIMEOUT_MS
  );
  if (status != PLATFORM_STATUS_OK) {
    return F103_TEST_RESULT_FAIL;
  }

  status = Platform_UART_Receive(
    PLATFORM_UART_3,
    response,
    sizeof(response),
    F103_TEST_UART_TIMEOUT_MS
  );
  if (status == PLATFORM_STATUS_TIMEOUT) {
    (void)Platform_UART_AbortReceive(PLATFORM_UART_3);
    return F103_TEST_RESULT_SKIP;
  }
  return status == PLATFORM_STATUS_OK
           ? F103_TEST_RESULT_PASS
           : F103_TEST_RESULT_FAIL;
}

static void f103_hardwaretest_send(const char *text) {
  if (text == NULL || s_uart_tx_busy) {
    return;
  }

  const size_t length = strlen(text);
  if (length == 0u || length >= sizeof(s_uart_tx)) {
    return;
  }

  memcpy(s_uart_tx, text, length);
  s_uart_tx_busy = true;
  if (
    Platform_UART_TransmitDMA(
      PLATFORM_UART_2,
      (const uint8_t *)s_uart_tx,
      (uint16_t)length
    ) != PLATFORM_STATUS_OK
  ) {
    s_uart_tx_busy = false;
  }
}

static void f103_hardwaretest_armuartreceive(void) {
  s_uart_rearm_pending = false;
  if (
    Platform_UART_ReceiveIT(
      PLATFORM_UART_2,
      &s_uart_rx_byte,
      1u
    ) != PLATFORM_STATUS_OK
  ) {
    s_uart_rearm_pending = true;
  }
}

static bool f103_hardwaretest_commandis(
  const char *command,
  const char *expected
) {
  return strncmp(command, expected, strlen(expected)) == 0;
}

static void f103_hardwaretest_handlecommand(const char *command) {
  if (f103_hardwaretest_commandis(command, "PING")) {
    f103_hardwaretest_send("PONG\r\n");
    return;
  }

  if (f103_hardwaretest_commandis(command, "TEST")) {
    f103_hardware_test_summary_t summary = F103_HardwareTest_RunAll();
    char response[96];
    f103_hardware_test_text_t text = {
      .data = response,
      .capacity = sizeof(response),
      .length = 0u,
    };
    response[0] = '\0';
    f103_hardwaretest_append(&text, "TEST pass=");
    f103_hardwaretest_appenduint(&text, summary.passed);
    f103_hardwaretest_append(&text, " fail=");
    f103_hardwaretest_appenduint(&text, summary.failed);
    f103_hardwaretest_append(&text, " skip=");
    f103_hardwaretest_appenduint(&text, summary.skipped);
    f103_hardwaretest_append(&text, "\r\n");
    f103_hardwaretest_send(response);
    return;
  }

  if (f103_hardwaretest_commandis(command, "RX_DMA")) {
    (void)Platform_UART_AbortReceive(PLATFORM_UART_2);
    memset(s_uart_dma_test, 0, sizeof(s_uart_dma_test));
    s_uart_rx_dma_complete = false;
    s_uart_rx_dma_active = true;
    const platform_status_t status = Platform_UART_ReceiveDMA(
      PLATFORM_UART_2,
      s_uart_dma_test,
      sizeof(s_uart_dma_test)
    );
    if (status == PLATFORM_STATUS_OK) {
      f103_hardwaretest_send("DMA READY\r\n");
    } else {
      s_uart_rx_dma_active = false;
      f103_hardwaretest_send("RX_DMA START FAILED\r\n");
      f103_hardwaretest_armuartreceive();
    }
    return;
  }

  if (f103_hardwaretest_commandis(command, "RESETFLAGS")) {
    char response[96];
    f103_hardware_test_text_t text = {
      .data = response,
      .capacity = sizeof(response),
      .length = 0u,
    };
    response[0] = '\0';
    f103_hardwaretest_append(&text, "RESETFLAGS iwdg=");
    f103_hardwaretest_appenduint(&text, s_boot_iwdg_reset ? 1u : 0u);
    f103_hardwaretest_append(&text, " wwdg=");
    f103_hardwaretest_appenduint(&text, s_boot_wwdg_reset ? 1u : 0u);
    f103_hardwaretest_append(&text, "\r\n");
    f103_hardwaretest_send(response);
    return;
  }

  if (f103_hardwaretest_commandis(command, "IWDG")) {
    (void)Platform_UART_Transmit(
      PLATFORM_UART_2,
      (const uint8_t *)"IWDG ARMED\r\n",
      12u,
      F103_TEST_UART_TIMEOUT_MS
    );
    const platform_status_t status = Platform_IWDG_Start(
      PLATFORM_IWDG_1,
      F103_TEST_IWDG_TIMEOUT_MS
    );
    SYS_LOG_INFO("HW", "iwdg start status=%lu", (unsigned long)status);
    if (status == PLATFORM_STATUS_OK) {
      for (;;) {
      }
    }

    char response[64];
    f103_hardware_test_text_t text = {
      .data = response,
      .capacity = sizeof(response),
      .length = 0u,
    };
    response[0] = '\0';
    f103_hardwaretest_append(&text, "IWDG START FAILED status=");
    f103_hardwaretest_appenduint(&text, status);
    f103_hardwaretest_append(&text, "\r\n");
    f103_hardwaretest_send(response);
    return;
  }

  if (f103_hardwaretest_commandis(command, "WWDG")) {
    const platform_wwdg_config_t config = {
      .timeout_ms = F103_TEST_WWDG_TIMEOUT_MS,
      .window = 0x50u,
    };
    (void)Platform_UART_Transmit(
      PLATFORM_UART_2,
      (const uint8_t *)"WWDG ARMED\r\n",
      12u,
      F103_TEST_UART_TIMEOUT_MS
    );
    const platform_status_t status = Platform_WWDG_Start(
      PLATFORM_WWDG_1,
      &config
    );
    SYS_LOG_INFO("HW", "wwdg start status=%lu", (unsigned long)status);
    if (status == PLATFORM_STATUS_OK) {
      for (;;) {
      }
    }

    char response[64];
    f103_hardware_test_text_t text = {
      .data = response,
      .capacity = sizeof(response),
      .length = 0u,
    };
    response[0] = '\0';
    f103_hardwaretest_append(&text, "WWDG START FAILED status=");
    f103_hardwaretest_appenduint(&text, status);
    f103_hardwaretest_append(&text, "\r\n");
    f103_hardwaretest_send(response);
  }
}

static void f103_hardwaretest_uartcallback(
  platform_uart_instance_t uart,
  const platform_uart_event_t *event,
  void *context
) {
  (void)context;
  if (uart != PLATFORM_UART_2 || event == NULL) {
    return;
  }

  if (event->type == PLATFORM_UART_EVENT_TX_COMPLETE) {
    s_uart_tx_busy = false;
    return;
  }

  if (event->type == PLATFORM_UART_EVENT_RX_COMPLETE) {
    if (s_uart_rx_dma_active) {
      s_uart_rx_dma_active = false;
      s_uart_rx_dma_complete = true;
      return;
    }

    const uint16_t next =
      (uint16_t)((s_uart_ring_head + 1u) % F103_TEST_UART_RING_SIZE);
    if (next != s_uart_ring_tail) {
      s_uart_ring[s_uart_ring_head] = s_uart_rx_byte;
      s_uart_ring_head = next;
    }
    if (
      Platform_UART_ReceiveIT(
        PLATFORM_UART_2,
        &s_uart_rx_byte,
        1u
      ) != PLATFORM_STATUS_OK
    ) {
      s_uart_rearm_pending = true;
    }
    return;
  }

  if (
    event->type == PLATFORM_UART_EVENT_ERROR ||
    event->type == PLATFORM_UART_EVENT_ABORT_RX_COMPLETE
  ) {
    s_uart_rearm_pending = true;
  }
}

static void f103_hardwaretest_adccallback(
  platform_adc_instance_t adc,
  const platform_adc_event_t *event,
  void *context
) {
  (void)context;
  if (
    adc == PLATFORM_ADC_1 &&
    event != NULL &&
    event->type == PLATFORM_ADC_EVENT_CONV_COMPLETE
  ) {
    s_adc_dma_complete = true;
  }
}

void F103_HardwareTest_Init(void) {
  s_boot_iwdg_reset = Platform_IWDG_WasReset(PLATFORM_IWDG_1);
  s_boot_wwdg_reset = Platform_WWDG_WasReset(PLATFORM_WWDG_1);

  const platform_uart_callback_binding_t uart_callbacks = {
    .callback = f103_hardwaretest_uartcallback,
    .context = NULL,
  };
  const platform_adc_callback_binding_t adc_callbacks = {
    .callback = f103_hardwaretest_adccallback,
    .context = NULL,
  };
  const platform_status_t uart_status = Platform_UART_RegisterCallback(
    PLATFORM_UART_2,
    &uart_callbacks
  );
  const platform_status_t adc_status = Platform_ADC_RegisterCallback(
    PLATFORM_ADC_1,
    &adc_callbacks
  );
  SYS_LOG_INFO(
    "HW",
    "platform init uart=%lu adc=%lu",
    (unsigned long)uart_status,
    (unsigned long)adc_status
  );

  f103_hardwaretest_armuartreceive();
  char boot_message[96];
  f103_hardware_test_text_t boot_text = {
    .data = boot_message,
    .capacity = sizeof(boot_message),
    .length = 0u,
  };
  boot_message[0] = '\0';
  f103_hardwaretest_append(&boot_text, "F103 READY iwdg=");
  f103_hardwaretest_appenduint(
    &boot_text,
    s_boot_iwdg_reset ? 1u : 0u
  );
  f103_hardwaretest_append(&boot_text, " wwdg=");
  f103_hardwaretest_appenduint(
    &boot_text,
    s_boot_wwdg_reset ? 1u : 0u
  );
  f103_hardwaretest_append(&boot_text, "\r\n");
  f103_hardwaretest_send(boot_message);
}

f103_hardware_test_summary_t F103_HardwareTest_RunAll(void) {
  f103_hardware_test_summary_t summary = {0u, 0u, 0u};

  SYS_LOG_INFO(
    "HW",
    "BEGIN reset_iwdg=%u reset_wwdg=%u",
    s_boot_iwdg_reset ? 1u : 0u,
    s_boot_wwdg_reset ? 1u : 0u
  );

  f103_hardwaretest_report(
    "platform_gpio_readback",
    f103_hardwaretest_checkgpio(),
    &summary
  );
  f103_hardwaretest_report(
    "platform_i2c_aht20",
    f103_hardwaretest_checki2caddress(F103_TEST_I2C_AHT20_ADDRESS),
    &summary
  );
  f103_hardwaretest_report(
    "platform_i2c_oled",
    f103_hardwaretest_checki2caddress(F103_TEST_I2C_OLED_ADDRESS),
    &summary
  );
  f103_hardwaretest_report(
    "platform_adc_dma",
    f103_hardwaretest_checkadc(),
    &summary
  );
  f103_hardwaretest_report(
    "platform_encoder_tim1",
    f103_hardwaretest_checkencoder(),
    &summary
  );
  f103_hardwaretest_report(
    "platform_pwm_tim2",
    f103_hardwaretest_checktim2(),
    &summary
  );
  f103_hardwaretest_report(
    "platform_pwm_tim3",
    f103_hardwaretest_checktim3(),
    &summary
  );
  f103_hardwaretest_report(
    "platform_pwm_tim4",
    f103_hardwaretest_checktim4(),
    &summary
  );
  f103_hardwaretest_report(
    "platform_uart3_probe",
    f103_hardwaretest_checkusart3(),
    &summary
  );

  SYS_LOG_INFO(
    "HW",
    "SUMMARY pass=%lu fail=%lu skip=%lu",
    (unsigned long)summary.passed,
    (unsigned long)summary.failed,
    (unsigned long)summary.skipped
  );
  return summary;
}

void F103_HardwareTest_Process(void) {
  if (s_uart_rx_dma_complete) {
    s_uart_rx_dma_complete = false;
    if (memcmp(s_uart_dma_test, "12345678", sizeof(s_uart_dma_test)) == 0) {
      f103_hardwaretest_send("RX_DMA PASS\r\n");
      SYS_LOG_INFO("HW", "platform_uart2_rx_dma PASS");
    } else {
      f103_hardwaretest_send("RX_DMA FAIL\r\n");
      SYS_LOG_WARN("HW", "platform_uart2_rx_dma FAIL");
    }
    f103_hardwaretest_armuartreceive();
  }

  if (s_uart_rearm_pending) {
    (void)Platform_UART_AbortReceive(PLATFORM_UART_2);
    f103_hardwaretest_armuartreceive();
  }

  while (s_uart_ring_tail != s_uart_ring_head) {
    const char byte = (char)s_uart_ring[s_uart_ring_tail];
    s_uart_ring_tail =
      (uint16_t)((s_uart_ring_tail + 1u) % F103_TEST_UART_RING_SIZE);

    if (byte == '\r') {
      continue;
    }
    if (byte == '\n') {
      if (s_uart_command_length != 0u) {
        s_uart_command[s_uart_command_length] = '\0';
        SYS_LOG_INFO("HW", "usart2 command=%s", s_uart_command);
        f103_hardwaretest_handlecommand(s_uart_command);
        s_uart_command_length = 0u;
      }
      continue;
    }

    if (s_uart_command_length < sizeof(s_uart_command) - 1u) {
      s_uart_command[s_uart_command_length] = byte;
      s_uart_command_length++;
    }
  }
}
