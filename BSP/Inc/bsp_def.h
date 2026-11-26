/**
 * @file bsp_def.h
 * @brief BSP 层统一状态码定义。
 * @addtogroup bsp
 * @{
 */

#ifndef LAYERED_CAR_BSP_DEF_H
#define LAYERED_CAR_BSP_DEF_H

/**
 * @brief BSP 公共接口返回状态码。
 */
typedef enum bsp_status_e {
  BSP_OK = 0U,
  BSP_ERR_INVALID_ARG,
  BSP_ERR_BUSY,
  BSP_ERR_TIMEOUT,
  BSP_ERR_NO_MEM,
  BSP_ERR_HAL,
  BSP_ERR_UNSUPPORTED,
} bsp_status_t;

/** @} */

#endif /* LAYERED_CAR_BSP_DEF_H */
