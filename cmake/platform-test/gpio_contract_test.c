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
 * @brief 查询测试替身接受的 GPIO 端口。
 * @param port 逻辑端口身份；测试替身只接受 PLATFORM_GPIO_PORT_A。
 * @retval true port 等于 PLATFORM_GPIO_PORT_A。
 * @retval false port 为其他枚举值。
 * @note 只比较枚举值，不解析 HAL 端口、不配置或读写引脚，也不修改全局状态。
 */
bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port) {
  return port == PLATFORM_GPIO_PORT_A;
}

/**
 * @brief 模拟读取 GPIO 引脚并固定报告高电平。
 * @param port 端口身份；本替身忽略该参数，不校验端口有效性。
 * @param pin 引脚掩码；本替身忽略该参数，不检查是否为零、单 bit 或端口范围。
 * @return 固定返回 PLATFORM_GPIO_PIN_SET，不读取任何 GPIO 输入寄存器。
 * @post 不修改引脚输出、配置或错误快照，也不产生可观察状态变化。
 * @note 这是测试替身；非法端口由公共层在调用本替身前拦截，本函数本身不模拟该错误。
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
 * @brief 记录最近一次 GPIO 写入请求，不驱动真实引脚。
 * @param port 端口身份；本替身忽略该参数，不校验端口有效性。
 * @param pin 引脚掩码；原样保存到 s_last_pin，不检查是否为零或单 bit。
 * @param state 目标逻辑电平；原样保存到 s_last_state，不检查枚举范围。
 * @post s_last_pin 等于 pin，s_last_state 等于 state；GPIO 输出寄存器和引脚电平不变。
 * @note 这是测试替身，供断言观察公共层传递的参数；不访问 HAL，也不产生错误快照。
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
 * @brief 记录最近一次 GPIO 翻转请求，不改变真实引脚。
 * @param port 端口身份；本替身忽略该参数，不校验端口有效性。
 * @param pin 引脚掩码；原样保存到 s_last_pin，不检查是否为零或单 bit。
 * @post s_last_pin 等于 pin，s_last_state 不变；GPIO 输出寄存器和引脚电平不变。
 * @note 这是测试替身，不访问 HAL，也不模拟真实翻转的读改写竞争。
 */
void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
) {
  (void)port;
  s_last_pin = pin;
}

/**
 * @brief 模拟锁定 GPIO 配置并固定报告成功。
 * @param port 端口身份；本替身忽略该参数，不校验端口有效性。
 * @param pin 引脚掩码；本替身忽略该参数，不检查是否为零或单 bit。
 * @retval PLATFORM_STATUS_OK 固定返回，但未写 GPIO 锁定寄存器。
 * @post 不锁定任何引脚、不修改配置，也不改变全局测试状态。
 * @note 测试替身不会返回生产后端声明的 ERROR、BUSY 或 TIMEOUT 分支。
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
 * @brief 验证 GPIO 读写记录和非法端口错误快照。
 * @post 成功返回时，端口 A 的读取返回 SET；写入端口 A 的引脚 2 后，
 *       s_last_pin 为 2、s_last_state 为 RESET；非法端口读取返回 RESET，
 *       且 GetLastError 保留 PLATFORM_ERROR_INVALID_ARGUMENT 位。
 * @warning 任一 assert 失败都会终止测试进程，不返回可恢复的错误状态。
 * @note 测试使用本文件的 GPIO 桩，不驱动真实引脚或观测外部电平。
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
 * @brief 运行 GPIO 契约测试并返回进程状态。
 * @return 固定返回 0；只有所有内层断言通过才会到达该返回语句。
 * @post 测试全局状态保留最后写入的引脚 2 和 RESET 电平。
 * @warning 任一 assert 失败都会终止进程，因此不会返回非零退出码。
 * @note 仅供 CTest 启动本测试可执行文件时调用。
 */
int main(void) {
  test_gpio_contract();
  return 0;
}
