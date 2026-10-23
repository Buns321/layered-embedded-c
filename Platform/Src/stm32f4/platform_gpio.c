//
// 由 Buns_ 创建于 2026/8/15。
//

#include "platform_gpio.h"
#include "platform_hal_internal.h"
#include <assert.h>
#include "stm32f4xx_hal.h"

/**
 * @brief  将传入的 platform 类型 port 参数映射为 HAL 类型 port
 *         是platform层内部函数，不可外部调用
 * @param  port (Platform_GPIO_Port 类型)
 * @return 转换后的 GPIO_TypeDef 类型
 */
static GPIO_TypeDef *platform_to_hal_port(Platform_GPIO_Port port) {
  switch (port) {
    case PLATFORM_GPIO_PORT_A:  return GPIOA;
    case PLATFORM_GPIO_PORT_B:  return GPIOB;
    case PLATFORM_GPIO_PORT_C:  return GPIOC;
#if PLATFORM_ENABLE_GPIO_D
    case PLATFORM_GPIO_PORT_D:  return GPIOD;
#endif
#if PLATFORM_ENABLE_GPIO_E
    case PLATFORM_GPIO_PORT_E:  return GPIOE;
#endif
    default:                    assert(0); return NULL;
  }
}

/**
 * @brief  将传入的 platform 类型 state 参数映射为 HAL 类型 state，
 *         是platform层内部函数，不可外部调用
 * @param  state (Platform_GPIO_Pin_State 类型)
 * @return 转换后的 GPIO_PinState 类型
 */
static GPIO_PinState platform_to_hal_pinstate(Platform_GPIO_Pin_State state) {
  switch (state) {
    case PLATFORM_GPIO_PIN_RESET: return GPIO_PIN_RESET;
    case PLATFORM_GPIO_PIN_SET:   return GPIO_PIN_SET;
    default:                      assert(0); return 0;
  }
}

/**
 * @brief  将传入的 HAL 类型 state 参数映射为 platform 类型 state，
 *         是platform层内部函数，不可外部调用
 * @param  state (HAL 类型)
 * @return 转换后的 platform 类型
 */
static Platform_GPIO_Pin_State hal_to_platform_pinstate(GPIO_PinState state) {
  switch (state) {
    case GPIO_PIN_RESET:  return PLATFORM_GPIO_PIN_RESET;
    case GPIO_PIN_SET:    return PLATFORM_GPIO_PIN_SET;
    default:              assert(0); return 0;
  }
}

Platform_GPIO_Pin_State Platform_GPIO_ReadPin (Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin) {
  GPIO_TypeDef *g = platform_to_hal_port(GPIOx);
  /* 检查入参 */
  assert(g != NULL);
  return  hal_to_platform_pinstate(HAL_GPIO_ReadPin(g, GPIO_Pin));
}

void Platform_GPIO_WritePin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin, Platform_GPIO_Pin_State PinState) {
  GPIO_TypeDef *g = platform_to_hal_port(GPIOx);
  /* 检查入参 */
  assert(g != NULL);
  HAL_GPIO_WritePin(g, GPIO_Pin, platform_to_hal_pinstate(PinState));
}

void Platform_GPIO_TogglePin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin) {
  GPIO_TypeDef *g = platform_to_hal_port(GPIOx);
  /* 检查入参 */
  assert(g != NULL);
  HAL_GPIO_TogglePin(g, GPIO_Pin);
}

Platform_StatusTypeDef Platform_GPIO_LockPin(Platform_GPIO_Port GPIOx, uint16_t GPIO_Pin) {
  GPIO_TypeDef *g = platform_to_hal_port(GPIOx);
  /* 检查入参 */
  assert(g != NULL);
  return platform_from_hal_status(HAL_GPIO_LockPin(g, GPIO_Pin));
}
