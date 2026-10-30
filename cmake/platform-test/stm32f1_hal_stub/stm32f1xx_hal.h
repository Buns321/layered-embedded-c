/**
 * @file stm32f1xx_hal.h
 * @brief 原生测试使用的最小 STM32F1 HAL 占位接口。
 * @ingroup tests
 */

#ifndef LAYERED_EMBEDDED_C_STM32F1XX_HAL_STUB_H
#define LAYERED_EMBEDDED_C_STM32F1XX_HAL_STUB_H

#include <stdint.h>

typedef enum hal_status_e {
  HAL_OK = 0,
  HAL_ERROR,
  HAL_BUSY,
  HAL_TIMEOUT,
} HAL_StatusTypeDef;

typedef struct iwdg_init_s {
  uint32_t Prescaler;
  uint32_t Reload;
} IWDG_InitTypeDef;

typedef struct iwdg_handle_s {
  const void *Instance;
  IWDG_InitTypeDef Init;
} IWDG_HandleTypeDef;

typedef struct wwdg_init_s {
  uint32_t Prescaler;
  uint32_t Window;
  uint32_t Counter;
  uint32_t EWIMode;
} WWDG_InitTypeDef;

typedef struct wwdg_handle_s {
  const void *Instance;
  WWDG_InitTypeDef Init;
} WWDG_HandleTypeDef;

typedef struct rcc_registers_s {
  volatile uint32_t CSR;
} RCC_Registers;

extern RCC_Registers g_stm32f1_test_rcc;

#define IWDG ((const void *)0x40003000u)
#define WWDG ((const void *)0x40002C00u)
#define RCC (&g_stm32f1_test_rcc)

#define IWDG_PRESCALER_4   0x00000000u
#define IWDG_PRESCALER_8   0x00000001u
#define IWDG_PRESCALER_16  0x00000002u
#define IWDG_PRESCALER_32  0x00000003u
#define IWDG_PRESCALER_64  0x00000004u
#define IWDG_PRESCALER_128 0x00000005u
#define IWDG_PRESCALER_256 0x00000006u

#define WWDG_PRESCALER_1 0x00000000u
#define WWDG_PRESCALER_2 0x00000080u
#define WWDG_PRESCALER_4 0x00000100u
#define WWDG_PRESCALER_8 0x00000180u

#define RCC_CSR_IWDGRSTF 0x20000000u
#define RCC_CSR_WWDGRSTF 0x10000000u
#define RCC_CSR_RMVF     0x01000000u

#define WWDG_EWI_DISABLE 0x00000000u
#define __HAL_RCC_WWDG_CLK_ENABLE() ((void)0)

/**
 * @brief 记录 IWDG 初始化参数并模拟 HAL 初始化成功。
 * @param iwdg_h HAL IWDG 句柄；仅读取 Init.Prescaler 与 Init.Reload。
 * @return 恒为 HAL_OK，表示测试桩接受本次初始化请求。
 * @pre iwdg_h 必须指向可访问的 IWDG_HandleTypeDef；传入 NULL 会触发 assert 并终止测试进程。
 * @post s_iwdg_prescaler 保存 HAL 预分频编码，s_iwdg_reload 保存重装值。
 * @note 测试桩不访问寄存器、不启动看门狗，也不校验参数范围。
 */
HAL_StatusTypeDef HAL_IWDG_Init(IWDG_HandleTypeDef *iwdg_h);

/**
 * @brief 模拟独立看门狗刷新成功。
 * @param iwdg_h HAL IWDG 句柄；测试桩只检查其非空。
 * @return 恒为 HAL_OK，不模拟刷新失败。
 * @pre iwdg_h 必须非空；否则 assert 终止测试进程。
 * @post 不修改捕获参数或外部硬件状态。
 * @note 测试桩没有喂狗时序、窗口或寄存器副作用。
 */
HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *iwdg_h);

/**
 * @brief 记录 WWDG 初始化参数并模拟 HAL 初始化成功。
 * @param wwdg_h HAL WWDG 句柄；仅读取 Init.Prescaler 与 Init.Window。
 * @return 恒为 HAL_OK，表示测试桩接受本次初始化请求。
 * @pre wwdg_h 必须指向可访问的 WWDG_HandleTypeDef；传入 NULL 会触发 assert 并终止测试进程。
 * @post s_wwdg_prescaler 保存 HAL 预分频编码，s_wwdg_window 保存窗口值。
 * @note 测试桩不访问寄存器、不启动看门狗，也不校验参数范围。
 */
HAL_StatusTypeDef HAL_WWDG_Init(WWDG_HandleTypeDef *wwdg_h);

/**
 * @brief 模拟窗口看门狗刷新成功。
 * @param wwdg_h HAL WWDG 句柄；测试桩只检查其非空。
 * @return 恒为 HAL_OK，不模拟刷新失败。
 * @pre wwdg_h 必须非空；否则 assert 终止测试进程。
 * @post 不修改捕获参数或外部硬件状态。
 * @note 测试桩没有喂狗时序、窗口或寄存器副作用。
 */
HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *wwdg_h);

/**
 * @brief 提供固定 APB1 时钟频率供看门狗后端计算。
 * @return APB1 时钟频率，恒为 36000000 Hz。
 * @post 不修改 RCC 寄存器或测试观察状态。
 * @note 该固定值是 STM32F1 测试时钟模型，不代表实际板级时钟配置。
 */
uint32_t HAL_RCC_GetPCLK1Freq(void);

#endif //LAYERED_EMBEDDED_C_STM32F1XX_HAL_STUB_H
