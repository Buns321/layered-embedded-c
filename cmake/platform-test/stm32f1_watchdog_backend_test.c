/**
 * @file stm32f1_watchdog_backend_test.c
 * @brief STM32F1 IWDG/WWDG HAL 分频编码回归测试。
 * @ingroup tests
 */

#include "platform_iwdg_backend.h"
#include "platform_wwdg_backend.h"
#include "stm32f1xx_hal.h"

#include <assert.h>
#include <stdint.h>

RCC_Registers g_stm32f1_test_rcc;

static uint32_t s_iwdg_prescaler;
static uint32_t s_iwdg_reload;
static uint32_t s_wwdg_prescaler;
static uint32_t s_wwdg_window;

/**
 * @brief 记录 IWDG 初始化参数并模拟 HAL 初始化成功。
 * @param iwdg_h HAL IWDG 句柄；仅读取 Init.Prescaler 与 Init.Reload。
 * @return 恒为 HAL_OK，表示测试桩接受本次初始化请求。
 * @pre iwdg_h 必须指向可访问的 IWDG_HandleTypeDef；传入 NULL 会触发 assert 并终止测试进程。
 * @post s_iwdg_prescaler 保存 HAL 预分频编码，s_iwdg_reload 保存重装值。
 * @note 测试桩不访问寄存器、不启动看门狗，也不校验参数范围。
 */
HAL_StatusTypeDef HAL_IWDG_Init(IWDG_HandleTypeDef *iwdg_h) {
  assert(iwdg_h != NULL);
  s_iwdg_prescaler = iwdg_h->Init.Prescaler;
  s_iwdg_reload = iwdg_h->Init.Reload;
  return HAL_OK;
}

/**
 * @brief 记录 WWDG 初始化参数并模拟 HAL 初始化成功。
 * @param wwdg_h HAL WWDG 句柄；仅读取 Init.Prescaler 与 Init.Window。
 * @return 恒为 HAL_OK，表示测试桩接受本次初始化请求。
 * @pre wwdg_h 必须指向可访问的 WWDG_HandleTypeDef；传入 NULL 会触发 assert 并终止测试进程。
 * @post s_wwdg_prescaler 保存 HAL 预分频编码，s_wwdg_window 保存窗口值。
 * @note 测试桩不访问寄存器、不启动看门狗，也不校验参数范围。
 */
HAL_StatusTypeDef HAL_WWDG_Init(WWDG_HandleTypeDef *wwdg_h) {
  assert(wwdg_h != NULL);
  s_wwdg_prescaler = wwdg_h->Init.Prescaler;
  s_wwdg_window = wwdg_h->Init.Window;
  return HAL_OK;
}

/**
 * @brief 模拟独立看门狗刷新成功。
 * @param iwdg_h HAL IWDG 句柄；测试桩只检查其非空。
 * @return 恒为 HAL_OK，不模拟刷新失败。
 * @pre iwdg_h 必须非空；否则 assert 终止测试进程。
 * @post 不修改捕获参数或外部硬件状态。
 * @note 测试桩没有喂狗时序、窗口或寄存器副作用。
 */
HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *iwdg_h) {
  assert(iwdg_h != NULL);
  return HAL_OK;
}

/**
 * @brief 模拟窗口看门狗刷新成功。
 * @param wwdg_h HAL WWDG 句柄；测试桩只检查其非空。
 * @return 恒为 HAL_OK，不模拟刷新失败。
 * @pre wwdg_h 必须非空；否则 assert 终止测试进程。
 * @post 不修改捕获参数或外部硬件状态。
 * @note 测试桩没有喂狗时序、窗口或寄存器副作用。
 */
HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *wwdg_h) {
  assert(wwdg_h != NULL);
  return HAL_OK;
}

/**
 * @brief 提供固定 APB1 时钟频率供看门狗后端计算。
 * @return APB1 时钟频率，恒为 36000000 Hz。
 * @post 不修改 RCC 寄存器或测试观察状态。
 * @note 该固定值是 STM32F1 测试时钟模型，不代表实际板级时钟配置。
 */
uint32_t HAL_RCC_GetPCLK1Freq(void) {
  return 36000000u;
}

/**
 * @brief 验证 F1 IWDG/WWDG 后端的分频编码与捕获参数。
 * @return 全部断言通过时返回 0。
 * @post 成功路径留下 IWDG 与 WWDG 分频、重装和窗口测试观测值。
 * @note 任一 assert 失败都会立即终止测试进程，不会返回失败状态。
 * @warning 本用例使用 HAL 桩验证编码映射，不验证真实看门狗启动或硬件时序。
 */
int main(void) {
  assert(
    platform_iwdg_backend_start(4u, 999u) == PLATFORM_STATUS_OK
  );
  assert(s_iwdg_prescaler == IWDG_PRESCALER_4);
  assert(s_iwdg_prescaler == 0u);
  assert(s_iwdg_reload == 999u);

  assert(
    platform_wwdg_backend_start(1u, 0x50u) == PLATFORM_STATUS_OK
  );
  assert(s_wwdg_prescaler == WWDG_PRESCALER_1);
  assert(s_wwdg_prescaler == 0u);
  assert(s_wwdg_window == 0x50u);
  return 0;
}
