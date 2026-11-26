/**
 * @file bsp_hal_internal.h
 * @brief BSP 内部 HAL 状态码转换辅助头文件。
 * @ingroup bsp
 */

#ifndef LAYERED_CAR_BSP_HAL_INTERNAL_H
#define LAYERED_CAR_BSP_HAL_INTERNAL_H

#include "bsp_def.h"
#include "bsp_mcu.h"

/**
 * @brief 将 STM32 HAL 状态码映射为 BSP 统一状态码。
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
