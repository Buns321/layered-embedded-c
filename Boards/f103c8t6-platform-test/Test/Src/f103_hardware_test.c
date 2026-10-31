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
#include "platform_pwm.h"
#include "platform_uart.h"
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

/**
 * @brief 把 NUL 结尾字符串追加到定长文本缓冲区并保持 NUL 终止。
 * @param text 目标文本对象；data 必须可写，capacity 必须大于 0，length 必须小于 capacity。
 * @param value 待追加的 NUL 结尾字符串；NULL 视为空串。
 * @pre text、text->data 非 NULL，且 text->length 小于 text->capacity。
 * @post text->data 在 text->length 处写入 NUL 终止符；空间不足时静默截断，长度不超过
 *       capacity - 1。
 * @note 本编译单元私有辅助函数；不分配内存，不检查 data/capacity/length 的一致性。
 * @warning 调用方必须维护 text 的不变量；若 capacity 为 0 或 length 已越界，
 *          函数会越界写入。
 */
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

/**
 * @brief 把 uint32_t 以无符号十进制追加到文本缓冲区。
 * @param text 目标文本对象；需满足 f103_hardwaretest_append() 的缓冲区不变量。
 * @param value 待格式化的取值，范围 0..4294967295，无单位。
 * @pre text 和 text->data 非 NULL，text->length 小于 text->capacity。
 * @post 追加不含前导零、最多 10 个数字的十进制表示；空间不足时按 append()
 *       规则截断，可能无法写入完整数字。
 * @note 本编译单元私有辅助函数；不写前导零，不写正负号，最终 NUL 由
 *       f103_hardwaretest_append() 维护。
 */
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

/**
 * @brief 按测试结果累加汇总计数并输出对应等级日志。
 * @param name 测试项名称，非 NULL 的 NUL 结尾字符串；调用期间保持有效。
 * @param result PASS、FAIL 或 SKIP；其他取值按默认分支计为失败。
 * @param summary 汇总对象；三个计数必须由调用方初始化并可写。
 * @pre name、summary 非 NULL，summary 计数器未溢出。
 * @post 恰好递增 passed、skipped 或 failed 之一，并尝试输出 INFO（PASS）或
 *       WARN（SKIP/FAIL）日志。
 * @note 本编译单元私有辅助函数；日志可能被过滤或丢弃，但计数不受影响。
 */
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

/**
 * @brief 对 5 个板级输出引脚做拉低/拉高回读验证。
 * @retval F103_TEST_RESULT_PASS 每个引脚在低、高两种写入后都能读回相同逻辑电平。
 * @retval F103_TEST_RESULT_FAIL 任一引脚回读不一致。
 * @pre GPIO 时钟和 PA7/PB0/PB5/PB9/PA11 的推挽输出配置已完成，且没有其他任务
 *      同时改写这些引脚。
 * @post 5 个引脚全部恢复为低电平；回读只验证 MCU 输出状态，不证明外接负载功能正常。
 * @note 引脚用途：PA7/PB0 三色灯、PB5 继电器、PB9 蜂鸣器、PA11 HC-SR04-P TRIG。
 * @warning 回读过程中会产生极短高电平脉冲：继电器可能轻微动作、蜂鸣器可能轻响、
 *          HC-SR04-P 可能收到不完整触发脉冲；确认负载安全后再运行。
 */
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

/**
 * @brief 用 Platform_I2C_Probe 探测指定 7 位地址是否存在应答。
 * @param address 7 位 I2C 地址，范围 0x00..0x7F；当前调用为 0x38（AHT20）和
 *        0x3D（CH1116 OLED）。
 * @retval F103_TEST_RESULT_PASS 在 2 次尝试内返回 PLATFORM_STATUS_OK。
 * @retval F103_TEST_RESULT_FAIL 超时、总线忙、参数非法或探测执行失败。
 * @pre MX_I2C1_Init() 已完成，PB6/PB7 上拉和外部设备供电正常。
 * @post 不读写设备寄存器，不改变 I2C 配置；总线上可能有最多 2 次地址探测，
 *       每次以 10 ms 超时阻塞，总计通常不超过约 20 ms。
 * @note 设备缺失不会记为 SKIP，而是计为 FAIL；AHT20 与 OLED 共用 I2C1。
 * @warning 探测为阻塞轮询，不得与同一 I2C1 的其他事务并发调用。
 */
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

/**
 * @brief 启动 ADC1 双通道 DMA 采样并检查完成、范围与错误状态。
 * @retval F103_TEST_RESULT_PASS StartDMA 成功，完成回调在 20 次 1 ms 等待内到达，两个
 *         12 位原始码值均不超过 0x0FFF，且 Platform_ADC_GetLastError() 为 NONE。
 * @retval F103_TEST_RESULT_FAIL 启动失败、等待超时、码值越界或存在 ADC/DMA 错误。
 * @pre F103_HardwareTest_Init() 已注册 ADC1 完成回调，MX_ADC1_Init() 已配置
 *      PA4/PA5 两通道扫描和 DMA1_Channel1；在 RTOS 任务上下文调用。
 * @post 启动成功后无论完成或超时都会尝试 StopDMA；s_adc_dma 保留本次启动前清零值
 *       或 DMA 已写入的样本，s_adc_dma_complete 表示本次是否观察到完成事件。
 *       启动失败时立即返回，不调用 StopDMA。
 * @note 等待使用 Sys_DelayMs(1) 循环最多 20 次，RTOS 构建中会主动让出 CPU；
 *       样本按 uint16_t 视图读取 PA4/PA5，通过条件不要求样本非零或电压合理。
 * @warning 若 ADC 回调未注册，本项会等到超时后返回 FAIL；若 StartDMA 本身失败
 *          则立即返回 FAIL。ADC1/DMA1_Channel1 不能被其他采样流程同时占用。
 */
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

/**
 * @brief 验证 TIM1 编码器可启动、计数器可写读且位宽为 16 位。
 * @retval F103_TEST_RESULT_PASS 启动、写入 1234、读回 1234、位宽 16 和
 *         GetLastError()==NONE 全部成立。
 * @retval F103_TEST_RESULT_FAIL 任一步状态失败或读回值/位宽不匹配。
 * @pre MX_TIM1_Init() 已按编码器模式配置 PA8/PA9；没有其他任务依赖或改写该计数器。
 * @post 启动成功后一定会调用 Stop 停止 TIM1 接口；计数器可能保留 1234，
 *       不恢复原值。启动失败时不执行后续操作。
 * @note 本编译单元私有辅助函数；不验证 A/B 相方向和增量，只验证 Platform
 *       计数器读写路径。
 * @warning 运行期间若编码器仍有物理转动，读回可能被硬件计数改变而误判失败；
 *          电机或底盘控制不得与本测试同时使用 TIM1。
 */
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

/**
 * @brief 设置占空比、启动通道、可选延时，再停止并清零比较值。
 * @param pwm PWM 定时器身份；必须由板级配置启用。
 * @param channel 通道身份；必须由 CubeMX 映射到目标引脚。
 * @param max_duty 占空比量程，必须大于 0；实际 CCR = ARR * duty / max_duty。
 * @param duty 目标占空比，内部夹紧到 max_duty；0 表示零占空比。
 * @param delay_ms 启动成功后的保持时间，单位 ms；0 表示不延时。
 * @retval F103_TEST_RESULT_PASS SetDuty、Start、Stop、SetCompare(0) 均返回 OK，
 *         且 GetLastError() 为 NONE。
 * @retval F103_TEST_RESULT_FAIL 任一状态非 OK 或存在 PWM 错误。
 * @pre MX_TIMx_Init() 已完成并配置为 PWM 模式；无其他任务同时控制同一通道。
 * @post 无论中间步骤是否成功，都会尝试 Stop 和 SetCompare(0)；全部成功时通道
 *       停止且 CCR 清零，失败时仅保证已尝试。延时只在 Start 成功且 delay_ms
 *       非 0 时发生。
 * @note 本编译单元私有辅助函数；TIM2/TIM3 调用方传 duty=0，TIM4 调用方传
 *       1500/20000 的短时中位脉冲；其他调用方不得假定输出始终为零。
 * @warning 若以非零 duty 调用，会真实驱动电机、WS2812 或舵机引脚；必须确认
 *          负载和电源安全。
 */
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

/**
 * @brief 用零占空比短时启动 TIM2_CH1 和 TIM2_CH2 并检查状态。
 * @retval F103_TEST_RESULT_PASS 两个通道的 exercisepwm 都返回 PASS。
 * @retval F103_TEST_RESULT_FAIL 任一通道失败。
 * @pre TIM2 已配置为 20 kHz PWM（ARR=3599），PA0/PA1 连接 DRV8833；电机处于安全状态。
 * @post 两个通道均执行 Stop 和 SetCompare(0)；全部成功时停止且 CCR 清零。
 * @note 本编译单元私有辅助函数；每通道使用 max_duty=3599、duty=0、2 ms 保持时间，
 *       只验证 Platform 路径，不验证电机实际转动。
 */
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

/**
 * @brief 用零占空比短时启动 TIM3_CH1 并检查状态。
 * @retval F103_TEST_RESULT_PASS exercisepwm 返回 PASS。
 * @retval F103_TEST_RESULT_FAIL exercisepwm 返回 FAIL。
 * @pre TIM3 已按 WS2812 时基配置（ARR=89，PB4 partial remap），且 CH2/CH3 未启用。
 * @post CH1 执行 Stop 和 SetCompare(0)；成功时停止且 CCR 清零，不会发送完整
 *       WS2812 帧。
 * @note 本编译单元私有辅助函数；使用 max_duty=89、duty=0、2 ms 保持时间，不验证
 *       灯珠颜色或单线时序。
 */
static f103_hardware_test_result_t f103_hardwaretest_checktim3(void) {
  return f103_hardwaretest_exercisepwm(
    PLATFORM_PWM_TIM3,
    PLATFORM_PWM_CHANNEL_1,
    89u,
    0u,
    2u
  );
}

/**
 * @brief 让 TIM4_CH3 输出短时中位脉冲并检查状态。
 * @retval F103_TEST_RESULT_PASS exercisepwm 返回 PASS。
 * @retval F103_TEST_RESULT_FAIL exercisepwm 返回 FAIL。
 * @pre TIM4 已配置为 50 Hz、ARR=19999 的舵机 PWM；PB8 连接的舵机处于空载或安全位置。
 * @post 舵机在约 20 ms 内可能收到 1 到 2 个 1500/20000 占空比脉冲，随后执行
 *       Stop 并清零 CCR；成功时停止输出脉冲。
 * @note 本编译单元私有辅助函数；max_duty=20000、duty=1500、delay_ms=20，约对应
 *       1.5 ms 中位脉冲。
 * @warning 会产生实际舵机运动；首次验证应空载并使用独立限流电源。
 */
static f103_hardware_test_result_t f103_hardwaretest_checktim4(void) {
  return f103_hardwaretest_exercisepwm(
    PLATFORM_PWM_TIM4,
    PLATFORM_PWM_CHANNEL_3,
    20000u,
    1500u,
    20u
  );
}

/**
 * @brief 向 USART3 发送 "AT" 加 CR、LF 并等待最多 32 字节响应。
 * @retval F103_TEST_RESULT_PASS 发送成功，且阻塞接收返回 PLATFORM_STATUS_OK
 *         （收到完整 32 字节）；不校验响应内容。
 * @retval F103_TEST_RESULT_SKIP 发送成功但接收超时；返回前中止接收。
 * @retval F103_TEST_RESULT_FAIL 发送失败或接收返回其他错误状态。
 * @pre MX_USART3_Init() 已完成（9600 8N1），DX-BT24 已连接并能返回至少 32 字节；
 *      无其他任务同时使用 USART3。
 * @post 超时路径会调用 AbortReceive 清除接收状态；成功和失败路径可能已消费
 *       外部设备发来的字节。
 * @note 本编译单元私有辅助函数；发送和接收各使用 100 ms 超时，最坏阻塞约 200 ms，
 *       接收内容被丢弃。
 * @warning 任意 32 字节响应都会判为 PASS，因此本项只能证明链路有数据，不能证明
 *          BLE 协议或 AT 命令语义正确。
 */
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

/**
 * @brief 通过 USART2 DMA 异步发送一条 NUL 结尾文本。
 * @param text 待发送文本，NUL 结尾；有效长度为 1..191 字节。NULL、空串或超长文本直接忽略。
 * @pre USART2 已完成初始化并注册回调；只在任务上下文调用，text 在复制完成前有效。
 * @post DMA 启动请求成功时把文本复制到静态 192 字节缓冲区、置 s_uart_tx_busy=true
 *       并启动 DMA；DMA 启动失败时清回 busy=false。busy 为真时后续调用直接丢弃。
 * @note 本编译单元私有辅助函数；TX_COMPLETE 回调负责清除 busy，若回调未注册或
 *       丢失，后续发送将永久被丢弃。
 * @warning 本函数不是线程安全的，且不等待发送完成；调用方不能假定返回时字节已上线。
 */
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

/**
 * @brief 清除重挂标志并在 USART2 上启动 1 字节 IT 接收。
 * @pre USART2 已初始化且回调已注册；不得在 DMA 接收仍活跃时调用。
 * @post 成功时 s_uart_rearm_pending=false 且 IT 接收已挂接；失败时置
 *       s_uart_rearm_pending=true，等待 Process 重试。
 * @note 本编译单元私有辅助函数；接收目标为静态单字节 s_uart_rx_byte，完成回调
 *       把它复制到环形缓冲区。
 * @warning 本函数不重试；若持续失败，Process 每轮都会 AbortReceive 后再尝试。
 */
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

/**
 * @brief 判断命令是否以指定前缀开头（区分大小写）。
 * @param command 已 NUL 终止的接收命令；不得为 NULL。
 * @param expected 期望的 NUL 终止前缀，通常为字面量 "PING" 等；不得为 NULL。
 * @retval true command 的前 strlen(expected) 个字符与 expected 完全相同。
 * @retval false 前缀不匹配，或 command 短于 expected。
 * @pre command、expected 非 NULL 且已 NUL 终止。
 * @post 只读两个字符串，不修改缓冲区和命令长度。
 * @note 本编译单元私有辅助函数；使用 strncmp 前缀比较，不是整串相等，例如
 *       "PINGX" 也会匹配 "PING"。
 * @warning 前缀匹配可能让带附加参数的命令命中基础命令；新增命令时需保证前缀
 *          不冲突。参数为 NULL 属于调用方违约，函数不做检查或断言。
 */
static bool f103_hardwaretest_commandis(
  const char *command,
  const char *expected
) {
  return strncmp(command, expected, strlen(expected)) == 0;
}

/**
 * @brief 解析并执行一条 USART2 测试命令。
 * @param command 已 NUL 终止的命令文本；不得为 NULL。
 * @pre 在任务上下文调用，F103_HardwareTest_Init() 已完成，USART2 收发可用。
 * @post PING 发送 "PONG"；TEST 运行 F103_HardwareTest_RunAll() 并发送汇总；
 *       RX_DMA 中止当前接收、清空 8 字节 DMA 缓冲并启动新的 8 字节 DMA 接收，
 *       成功时发送 "DMA READY"，启动失败时发送 "RX_DMA START FAILED" 并恢复
 *       IT 接收。未知命令静默忽略，不回复也不改变状态。
 * @note 本编译单元私有辅助函数；命令按前缀匹配且区分大小写。PING/TEST/RX_DMA
 *       的响应及失败状态经 f103_hardwaretest_send() 非阻塞发送，忙时可能被丢弃。
 * @warning TEST 会按 F103_HardwareTest_RunAll() 的契约产生硬件副作用并阻塞；
 *          RX_DMA 后需由上位机发送恰好 8 字节 "12345678"。
 */
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

}

/**
 * @brief Platform UART2 事件回调：维护发送忙标志和接收环形缓冲区。
 * @param uart 事件来源实例；仅处理 PLATFORM_UART_2。
 * @param event 事件数据；仅在本次回调期间有效，NULL 时直接返回。
 * @param context 注册时传入的上下文；本测试固定为 NULL，未使用。
 * @pre 由 Platform UART 在中断上下文调用，event 指针仅本次有效；回调拥有 USART2。
 * @post TX_COMPLETE 清除 s_uart_tx_busy。RX_COMPLETE 在 DMA 激活时置
 *       s_uart_rx_dma_complete 并结束 DMA 状态；否则把 s_uart_rx_byte 写入环形
 *       缓冲区（满则丢弃）并重挂 1 字节 IT 接收，重挂失败置 s_uart_rearm_pending。
 *       ERROR/ABORT_RX_COMPLETE 仅置重挂标志；其他事件不改变状态。
 * @note 回调不复制 event，不打印日志、不阻塞、不分配内存；处理后立即返回。
 * @warning 环形缓冲区为 64 字节，满时新字节被静默丢弃；DMA 与 IT 接收共用
 *          USART2，调用方不能同时启动第二路接收。
 */
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

/**
 * @brief Platform ADC1 完成回调：记录 DMA 采样完成。
 * @param adc 事件来源实例；仅处理 PLATFORM_ADC_1。
 * @param event 事件数据；仅在本次回调期间有效，NULL 时忽略。
 * @param context 注册时传入的上下文；本测试固定为 NULL，未使用。
 * @pre 由 Platform ADC 在中断上下文调用；回调已通过 F103_HardwareTest_Init() 注册。
 * @post 仅当事件为 PLATFORM_ADC_EVENT_CONV_COMPLETE 时置 s_adc_dma_complete=true；
 *       其他实例或事件不改变状态。
 * @note 不读取样本、不停止 DMA、不打印日志；样本由 f103_hardwaretest_checkadc()
 *       在任务上下文读取。
 * @warning 半完成和错误事件均不会置位完成标志；检查函数只能依赖等待超时和
 *          GetLastError() 快照判断失败。
 */
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

/**
 * @brief 注册 USART2/ADC 回调并发送上电信息。
 * @pre 所有 CubeMX 外设初始化完成、RTOS 调度器已启动，且在任务上下文调用；
 *      USART2 和 ADC1 已按测试固件配置启用。
 * @post 按注册结果配置 UART2/ADC1 回调；尝试以单字节 IT 接收启动 USART2；
 *       尝试通过 DMA 发送 "F103 READY" 上电信息，发送失败时仅清除忙标志且不重试。
 * @note 回调注册失败只记录 SYS_LOG，不中止初始化；后续命令收发可能不可用。
 * @note 该函数无返回值，调用方必须通过日志、F103_HardwareTest_RunAll() 或
 *       USART2 命令观察结果。
 * @warning 应在调度器启动后调用一次；重复调用会使回调注册返回 BUSY，并可能
 *          使 USART2 接收重挂失败而进入待重挂状态。
 */
void F103_HardwareTest_Init(void) {
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
  f103_hardwaretest_send("F103 READY\r\n");
}

/**
 * @brief 依次执行 GPIO、I2C、ADC、编码器、PWM 和 USART3 共 9 项测试。
 * @return 本次运行的汇总：passed、failed、skipped 为各项计数之和（总项数为 9）；
 *         返回值为值拷贝，调用方拥有该副本。
 * @pre 已调用 F103_HardwareTest_Init() 注册 ADC1 完成回调，且测试固件已启用
 *      I2C1、ADC1、TIM1-TIM4、USART3；在任务上下文调用。
 * @post 输出 BEGIN 和 SUMMARY 日志；各子测试结束后的硬件状态见各自说明。
 * @note 执行顺序固定：GPIO 回读、AHT20、OLED、ADC DMA、TIM1 编码器、
 *       TIM2、TIM3、TIM4、USART3 探测。
 * @warning 会短暂拉高 PA7/PB0/PB5/PB9/PA11、改写 TIM1 计数器为 1234 并停止编码器、
 *          让 TIM4_CH3 输出约 1.5 ms 中位脉冲，且 USART3 收发可能各阻塞 100 ms。
 *          执行前必须确认继电器、蜂鸣器、舵机和编码器连接处于安全状态。
 * @warning 未调用 Init 时 ADC 项会因缺少完成回调在约 20 ms 后失败，UART3 结果也
 *          取决于外部设备；本函数不返回单独的错误码，失败仅体现在计数和日志中。
 */
f103_hardware_test_summary_t F103_HardwareTest_RunAll(void) {
  f103_hardware_test_summary_t summary = {0u, 0u, 0u};

  SYS_LOG_INFO("HW", "BEGIN");

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

/**
 * @brief 单次处理 USART2 DMA 接收结果、重挂请求和命令行输入。
 * @pre 已调用 F103_HardwareTest_Init() 且 USART2 回调已注册；必须在任务上下文
 *      周期调用（当前默认任务每 100 ms 一次），不得在 ISR 中调用。
 * @post 消费环形缓冲区中当前可读字节；以 LF 结束的非空命令会先写日志再执行，
 *       并清空命令长度。DMA 完成时输出 RX_DMA PASS/FAIL 并重新挂接收；
 *       重挂标志置位时先中止 USART2 接收再重新挂接。
 * @note 忽略 CR；超长命令仅保留前 63 个字符，其余字节在下一个 LF 前丢弃。
 * @note 不主动轮询串口硬件，只处理回调已写入的状态；若调用间隔过长，64 字节
 *       环形缓冲区会满并静默丢弃新字节。
 * @warning 执行 TEST 命令会运行完整硬件套件。
 */
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
