/**
 * @file gpio_contract_test.c
 * @brief GPIO Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_gpio_backend.h"
#include "platform_gpio.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint16_t s_last_pin;
static platform_gpio_pin_state_t s_last_state;

/**
 * @brief 校验 GPIO 端口是否已由板级配置启用。
 */
bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port) {
  return port == PLATFORM_GPIO_PORT_A;
}

/**
 * @brief 读取GPIO数据。
 */
platform_gpio_pin_state_t platform_gpio_backend_read(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  (void)pin;
  return PLATFORM_GPIO_PIN_SET;
}

/**
 * @brief 写入GPIO数据。
 */
void platform_gpio_backend_write(
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t state
) {
  (void)port;
  s_last_pin = pin;
  s_last_state = state;
}

/**
 * @brief 翻转 GPIO 输出电平。
 */
void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  s_last_pin = pin;
}

/**
 * @brief 锁定 GPIO 引脚配置，禁止运行期重配置。
 */
platform_status_t platform_gpio_backend_lock(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  (void)pin;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 验证 GPIO 参数校验与读写。
 */
static void test_gpio_contract(void) {
  assert(Platform_GPIO_ReadPin(PLATFORM_GPIO_PORT_A, 1U) ==
         PLATFORM_GPIO_PIN_SET);
  Platform_GPIO_WritePin(
    PLATFORM_GPIO_PORT_A,
    2U,
    PLATFORM_GPIO_PIN_RESET
  );
  assert(s_last_pin == 2U);
  assert(s_last_state == PLATFORM_GPIO_PIN_RESET);

  assert(Platform_GPIO_ReadPin((platform_gpio_port_t)99U, 1U) ==
         PLATFORM_GPIO_PIN_RESET);
  assert((Platform_GPIO_GetLastError((platform_gpio_port_t)99U) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 */
int main(void) {
  test_gpio_contract();
  return 0;
}
