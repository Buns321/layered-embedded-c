/**
 * @file bsp_hal_internal.h
 * @brief BSP 内部 HAL 状态码转换辅助头文件。
 * @details 仅限 `BSP/Src/` 内部包含，用于将 STM32 HAL 状态枚举统一映射为 `bsp_status_t`，
 *          避免上层头文件暴露 HAL_StatusTypeDef 依赖。
 * @ingroup BSP
 */

#ifndef LAYERED_CAR_BSP_HAL_INTERNAL_H
#define LAYERED_CAR_BSP_HAL_INTERNAL_H

#include "bsp_def.h"
#include "bsp_mcu.h"

/**
 * @brief 将 STM32 HAL 状态码映射为 BSP 统一状态码。
 * @param[in] status STM32 HAL 函数返回的原始状态码。
 * @return 对应的 `bsp_status_t` 状态码。
 * @retval BSP_OK          对应 HAL_OK。
 * @retval BSP_ERR_BUSY    对应 HAL_BUSY。
 * @retval BSP_ERR_TIMEOUT 对应 HAL_TIMEOUT。
 * @retval BSP_ERR_HAL     对应 HAL_ERROR 或其他未知值。
 */
static inline bsp_status_t bsp_status_from_hal(HAL_StatusTypeDef status) {
  switch (status) {
  case HAL_OK:
    return BSP_OK;
  case HAL_BUSY:
    return BSP_ERR_BUSY;
  case HAL_TIMEOUT:
    return BSP_ERR_TIMEOUT;
  case HAL_ERROR:
  default:
    return BSP_ERR_HAL;
  }
}

#endif /* LAYERED_CAR_BSP_HAL_INTERNAL_H */
