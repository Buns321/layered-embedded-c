//
// 由 Buns_ 创建于 2026/8/15。
//

/**
 * @file led_gpio.c
 * @brief GPIO LED 驱动实现。
 * @ingroup drivers_concrete
 */

#include "led_gpio.h"
#include <assert.h>

/**
 * @brief 将 GPIO LED 引脚驱动到配置的点亮电平。
 * @param led_h GPIO LED 的 led_base 视图；非 NULL，且必须来自已初始化的 led_gpio_t 首位成员。
 * @pre led_h 已由 Dev_LED_GPIO_Init 初始化，active_level 为有效枚举值。
 * @pre 仅供 Dev_LED_Base_TurnOn 通过 led_ops_gpio 调用，不得绕过基类单独依赖软件状态。
 * @post 引脚被写为 active_level；不修改 led_base.is_on、亮度或对象所有权。
 * @note GPIO 写接口无返回状态；非法 active_level 在调试构建触发 assert，在定义 NDEBUG 的构建中无动作。
 * @warning 不含锁或临界区；同一引脚不得由 ISR 和其他任务并发写入。
 */
static void dev_led_gpio_turnon(led_base_t *led_h) {
  assert(led_h);
  led_gpio_t *led_gpio_h = (led_gpio_t *)led_h;
  switch (led_gpio_h->active_level) {
    case PLATFORM_GPIO_PIN_SET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_SET);
      break;
    case PLATFORM_GPIO_PIN_RESET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_RESET);
      break;
    default: assert(0); break;
  }
}

/**
 * @brief 将 GPIO LED 引脚驱动到与点亮电平相反的电平。
 * @param led_h GPIO LED 的 led_base 视图；非 NULL，且必须来自已初始化的 led_gpio_t 首位成员。
 * @pre led_h 已由 Dev_LED_GPIO_Init 初始化，active_level 为有效枚举值。
 * @pre 仅供 Dev_LED_Base_TurnOff 通过 led_ops_gpio 调用，不得绕过基类单独依赖软件状态。
 * @post 引脚被写为 active_level 的反相电平；不修改 led_base.is_on、亮度或对象所有权。
 * @note GPIO 写接口无返回状态；非法 active_level 在调试构建触发 assert，在定义 NDEBUG 的构建中无动作。
 * @warning 不含锁或临界区；同一引脚不得由 ISR 和其他任务并发写入。
 */
static void dev_led_gpio_turnoff(led_base_t *led_h) {
  assert(led_h);
  led_gpio_t *led_gpio_h = (led_gpio_t *)led_h;
  switch (led_gpio_h->active_level) {
    case PLATFORM_GPIO_PIN_SET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_RESET);
      break;
    case PLATFORM_GPIO_PIN_RESET:
      Platform_GPIO_WritePin(led_gpio_h->port, led_gpio_h->pin, PLATFORM_GPIO_PIN_SET);
      break;
    default: assert(0); break;
  }
}

/**
 * @brief 拒绝 GPIO LED 的调光请求。
 * @param led_h GPIO LED 的 led_base 视图；非 NULL，但在本实现中不访问其字段。
 * @param brightness 请求亮度，约定范围 0..UINT16_MAX；GPIO LED 不支持该值，函数不保存也不映射它。
 * @pre 只能由 Dev_LED_Base_SetBrightness 经 led_ops_gpio 进入；GPIO LED 应改用 TurnOn/TurnOff。
 * @post 调试构建在 assert(0) 处中止且不改变引脚或 is_on；定义 NDEBUG 时为空操作，硬件和对象字段均不变。
 * @note 本实现不分配内存、不访问硬件；发布构建中调用基类 SetBrightness 会先返回此处，再把 is_on 更新为 brightness 是否非零，造成软件状态与实际引脚不一致。
 * @warning 不支持调光；不得把该回调当作 PWM 亮度接口使用，发布构建也不会报告此不匹配。
 */
static void dev_led_gpio_setbrightness(led_base_t *led_h, uint16_t brightness) {
  // 空函数用于调用参数，防止编译(编译前期的语法/语义分析阶段)产生 unused parameter 警告
  // 由于优化器作用与编译后期，因此空语句被优化不影响防止产生 unused parameter 警告的作用
  // 不可使用 assert(led_h && brightness) 语句以求防止产生 unused parameter 警告，这是因为在发布阶段定义 NODEBUG(Release) 后，assert() 会被展开为 (void)0
  (void)led_h; (void)brightness; assert(0);
}

static const led_ops_t led_ops_gpio = {
  .on             = dev_led_gpio_turnon,
  .off            = dev_led_gpio_turnoff,
  .set_brightness = dev_led_gpio_setbrightness,
};

/**
 * @brief 初始化 GPIO LED 并绑定 LED 基类操作表。
 * @param led_h 待初始化对象；非 NULL，生命周期和独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 led_base.name 保存指针而不复制。
 * @param port GPIO 端口身份；必须是当前构建启用的 platform_gpio_port_t。
 * @param pin 单引脚掩码；不可为 0，也不可同时包含多个有效引脚位。
 * @param active_level 点亮电平，只能为 PLATFORM_GPIO_PIN_SET 或 PLATFORM_GPIO_PIN_RESET。
 * @pre 对应 GPIO 已在 CubeMX/Core 中完成时钟、模式和速度配置，且调用发生在硬件使用之前。
 * @pre led_h 未被其他执行上下文使用；led_h 和 name 为 NULL 时由调试断言拦截。
 * @post led_base 已绑定 GPIO 操作表并记录 DEV_LED_OFF，port/pin/active_level 已保存，引脚被写为 active_level 的反相电平。
 * @note 不分配或释放内存，不复制 name；本函数不验证引脚当前是否被其他外设占用。
 * @note 底层 GPIO 写操作不返回状态；引脚或端口配置错误需通过 Platform_GPIO_GetLastError 检查。
 * @warning 非 ISR 安全；初始化、开关和调光不得与同一 led_h 或同一引脚的其他访问并发。非法 active_level 不在此处断言，但会使后续开关操作在调试构建中止。
 */
void Dev_LED_GPIO_Init(led_gpio_t *led_h, const char *name, platform_gpio_port_t port, uint16_t pin, platform_gpio_pin_state_t active_level) {
  assert(led_h && name);
  Dev_LED_Base_Init(&led_h->led_base, name, &led_ops_gpio);
  led_h->port = port;
  led_h->pin = pin;
  led_h->active_level = active_level;

  platform_gpio_pin_state_t inactive_level =
    (active_level == PLATFORM_GPIO_PIN_SET) ? PLATFORM_GPIO_PIN_RESET : PLATFORM_GPIO_PIN_SET;
  Platform_GPIO_WritePin(port, pin, inactive_level);
}
