/**
 * @file platform_gpio_backend.h
 * @brief STM32 系列公共 GPIO 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H

#include "platform_gpio.h"
#include "platform_def.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 判断 GPIO 端口身份是否对应当前板级配置启用的端口。
 * @param port 逻辑端口身份；有效集合为 A、B、C 以及编译期启用的 D、E。
 * @retval true port 已映射到当前系列的 HAL GPIO 端口。
 * @retval false port 越界或该端口未被板级配置启用。
 * @note 纯查询，不配置或读写引脚；无共享状态修改，可在任务或中断上下文调用。
 */
bool platform_gpio_backend_is_valid_port(platform_gpio_port_t port);

/**
 * @brief 直接读取单个 GPIO 引脚的逻辑电平。
 * @param port 有效端口身份；必须已由 CubeMX 配置为可读取的 GPIO。
 * @param pin 非零单引脚掩码，通常为 GPIO_PIN_0 到 GPIO_PIN_15 之一；本后端不拆分多 bit 掩码。
 * @return PLATFORM_GPIO_PIN_SET 表示 HAL 读到高电平；否则返回 PLATFORM_GPIO_PIN_RESET。
 * @pre port 已启用且 pin 对目标端口有效；公共 GPIO 层已完成这些校验。
 * @post 不修改引脚输出、配置或错误快照；函数只读取 GPIO 输入寄存器。
 * @warning 无效 port 会把 NULL 传给 HAL，多 bit 掩码的返回值由 HAL 行为决定；两者均非本后端契约。
 * @note 无阻塞、分配或内部锁，可在任务或中断上下文调用。
 */
platform_gpio_pin_state_t platform_gpio_backend_read(
  platform_gpio_port_t port,
  uint16_t pin
);

/**
 * @brief 直接设置单个 GPIO 引脚的输出电平。
 * @param port 有效端口身份；必须已由 CubeMX 配置为输出。
 * @param pin 非零单引脚掩码，通常为 GPIO_PIN_0 到 GPIO_PIN_15 之一；本后端不拆分多 bit 掩码。
 * @param state 目标逻辑电平；SET 输出高电平，RESET 输出低电平；其他枚举值在本后端被当作 RESET。
 * @pre port 已启用且 pin、state 已由公共 GPIO 层校验为合法值。
 * @post 函数返回时目标引脚的输出锁存器和引脚电平已更新；不产生状态码或错误快照。
 * @warning 本后端不检查空端口、引脚掩码或枚举范围，非法输入会交给 HAL；同一引脚的并发写
 *          以最后一次写为准，不同引脚通过独立的置位/复位寄存器位写入。
 * @note 仅执行寄存器写，不阻塞、不分配内存，可在任务或中断上下文调用。
 */
void platform_gpio_backend_write(
  platform_gpio_port_t port,
  uint16_t pin,
  platform_gpio_pin_state_t state
);

/**
 * @brief 直接翻转单个 GPIO 引脚的输出电平。
 * @param port 有效端口身份；必须已由 CubeMX 配置为输出。
 * @param pin 非零单引脚掩码，通常为 GPIO_PIN_0 到 GPIO_PIN_15 之一；本后端不拆分多 bit 掩码。
 * @pre port 已启用且 pin 对目标端口有效；公共 GPIO 层已完成这些校验。
 * @post 函数返回时目标引脚的输出锁存器取反；不产生状态码或错误快照。
 * @warning HAL 通过读改写输出寄存器完成翻转；同一端口的并发翻转可能丢失更新，调用方需自行串行化。
 * @note 无阻塞、分配或内部锁，可在任务或中断上下文调用。
 */
void platform_gpio_backend_toggle(
  platform_gpio_port_t port,
  uint16_t pin
);

/**
 * @brief 通过 GPIO 锁定寄存器永久锁定所选引脚的配置。
 * @param port 有效端口身份；引脚复用和电气配置必须已经完成并生效。
 * @param pin 非零单引脚掩码；锁定后该引脚配置不能再由软件修改。
 * @retval PLATFORM_STATUS_OK 锁定序列成功，目标引脚配置保持当前值直至复位。
 * @retval PLATFORM_STATUS_ERROR HAL 锁定失败或寄存器值未确认。
 * @retval PLATFORM_STATUS_BUSY HAL 报告忙时原样映射。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 报告超时时原样映射。
 * @pre port 已启用且 pin 对目标端口有效；公共 GPIO 层已完成校验。
 * @post 成功后目标引脚配置被锁定；失败时是否已部分写入锁定位由 HAL 硬件序列决定。
 * @warning 锁定一直持续到 MCU 复位，不是软件互斥锁；应在所有运行期配置完成后调用。
 * @note 调用会访问并确认 GPIO 配置锁定寄存器，不分配内存；仅允许在任务上下文执行。
 */
platform_status_t platform_gpio_backend_lock(
  platform_gpio_port_t port,
  uint16_t pin
);

#endif //LAYERED_EMBEDDED_C_PLATFORM_GPIO_BACKEND_H
