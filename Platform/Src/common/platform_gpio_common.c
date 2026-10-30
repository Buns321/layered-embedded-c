/**
 * @file platform_gpio_common.c
 * @brief 与芯片系列无关的 GPIO 参数校验与错误快照。
 * @ingroup platform
 */

#include "platform_error_internal.h"
#include "platform_gpio_backend.h"

static volatile uint32_t gpio_error[5];

/**
 * @brief 记录 GPIO 参数错误并构造统一失败状态。
 * @param port 端口身份；仅当该值位于内部快照数组范围内时才写入错误位。
 * @return 始终返回 PLATFORM_STATUS_INVALID_ARGUMENT。
 * @post 合法端口对应的错误快照置入 PLATFORM_ERROR_INVALID_ARGUMENT；
 *       非法端口不访问快照数组。
 * @note 仅供本文件失败路径使用；不访问 GPIO 硬件，不分配内存。
 */
static platform_status_t platform_gpio_invalid(
  platform_gpio_port_t port
) {
  if ((uint32_t)port < 5U) {
    platform_error_record(
      &gpio_error[port],
      PLATFORM_ERROR_INVALID_ARGUMENT
    );
  }
  return PLATFORM_STATUS_INVALID_ARGUMENT;
}

/**
 * @brief 校验 GPIO 引脚状态枚举。
 * @param state 待校验的 GPIO 逻辑电平枚举。
 * @return state 为 SET 或 RESET 时返回 true，其他值返回 false。
 * @note 纯函数；用于在写入硬件前拒绝未知枚举值。
 */
static bool platform_gpio_state_valid(platform_gpio_pin_state_t state) {
  return state == PLATFORM_GPIO_PIN_SET ||
         state == PLATFORM_GPIO_PIN_RESET;
}

/**
 * @brief 读取 GPIO 引脚电平并刷新该端口的错误快照。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @retval PLATFORM_GPIO_PIN_SET 读到高电平。
 * @retval PLATFORM_GPIO_PIN_RESET 读到低电平，或端口/掩码非法。
 * @note 非法端口或零掩码会记录 PLATFORM_ERROR_INVALID_ARGUMENT，并返回 RESET；
 *       调用方需要区分时应读取 Platform_GPIO_GetLastError()。
 * @note 多 bit 掩码的读取语义由 HAL 决定；本接口不分配内存且不阻塞。
 */
platform_gpio_pin_state_t Platform_GPIO_ReadPin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U
  ) {
    (void)platform_gpio_invalid(GPIOx);
    return PLATFORM_GPIO_PIN_RESET;
  }
  return platform_gpio_backend_read(GPIOx, GPIO_Pin);
}

/**
 * @brief 写入 GPIO 引脚逻辑电平。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @param PinState 目标逻辑电平；取 PLATFORM_GPIO_PIN_RESET 或 PLATFORM_GPIO_PIN_SET。
 * @post 输入有效时目标引脚被写入 PinState；端口、掩码或枚举非法时硬件不变，
 *       并记录 PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 无返回值。失败只能通过 Platform_GPIO_GetLastError() 观察。
 * @note 不分配内存、不阻塞；多 bit 掩码的写入语义由 HAL 决定。
 */
void Platform_GPIO_WritePin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin,
  platform_gpio_pin_state_t PinState
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U ||
    !platform_gpio_state_valid(PinState)
  ) {
    (void)platform_gpio_invalid(GPIOx);
    return;
  }
  platform_gpio_backend_write(GPIOx, GPIO_Pin, PinState);
}

/**
 * @brief 翻转 GPIO 引脚逻辑电平。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @post 输入有效时目标引脚电平翻转；端口或掩码非法时硬件不变并记录
 *       PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 不分配内存、不阻塞；并发翻转同一引脚的结果取决于调用顺序。
 */
void Platform_GPIO_TogglePin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U
  ) {
    (void)platform_gpio_invalid(GPIOx);
    return;
  }
  platform_gpio_backend_toggle(GPIOx, GPIO_Pin);
}

/**
 * @brief 锁定 GPIO 引脚配置，阻止后续修改。
 * @param GPIOx 已由板级配置启用的端口身份。
 * @param GPIO_Pin 非零引脚掩码；正常使用应只包含一个 bit。
 * @retval PLATFORM_STATUS_OK 引脚配置锁定成功。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 端口非法、掩码为零或已锁定位图无效。
 * @retval PLATFORM_STATUS_ERROR HAL 锁定失败。
 * @warning GPIO 锁定不可逆，重新上电前不能解锁。
 * @note 应由任务上下文调用并与其他 GPIO 配置操作串行化；不分配内存。
 */
platform_status_t Platform_GPIO_LockPin(
  platform_gpio_port_t GPIOx,
  uint16_t GPIO_Pin
) {
  if ((uint32_t)GPIOx < 5U) {
    platform_error_reset(&gpio_error[GPIOx]);
  }
  if (
    !platform_gpio_backend_is_valid_port(GPIOx) ||
    GPIO_Pin == 0U
  ) {
    return platform_gpio_invalid(GPIOx);
  }

  platform_status_t status =
    platform_gpio_backend_lock(GPIOx, GPIO_Pin);
  if (status != PLATFORM_STATUS_OK) {
    platform_error_record(&gpio_error[GPIOx], platform_error_from_status(status));
  }
  return status;
}

/**
 * @brief 读取最近一次 GPIO 操作错误快照。
 * @param GPIOx 端口身份；必须位于内部错误快照支持的 0..4，越界值返回
 *        PLATFORM_ERROR_INVALID_ARGUMENT。
 * @return 该端口的 PLATFORM_ERROR_* 位掩码；非法端口返回
 *         PLATFORM_ERROR_INVALID_ARGUMENT。
 * @note 只读，不清除快照；后续公共 GPIO 操作开始时会清除对应端口快照。
 * @note 不访问硬件、不分配内存；并发读写同一端口快照须由调用方串行化。
 */
uint32_t Platform_GPIO_GetLastError(platform_gpio_port_t GPIOx) {
  return (uint32_t)GPIOx < 5U
           ? gpio_error[GPIOx]
           : PLATFORM_ERROR_INVALID_ARGUMENT;
}
