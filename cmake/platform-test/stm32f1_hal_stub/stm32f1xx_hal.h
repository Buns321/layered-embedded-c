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

HAL_StatusTypeDef HAL_IWDG_Init(IWDG_HandleTypeDef *iwdg_h);
HAL_StatusTypeDef HAL_IWDG_Refresh(IWDG_HandleTypeDef *iwdg_h);
HAL_StatusTypeDef HAL_WWDG_Init(WWDG_HandleTypeDef *wwdg_h);
HAL_StatusTypeDef HAL_WWDG_Refresh(WWDG_HandleTypeDef *wwdg_h);
uint32_t HAL_RCC_GetPCLK1Freq(void);

#endif //LAYERED_EMBEDDED_C_STM32F1XX_HAL_STUB_H
