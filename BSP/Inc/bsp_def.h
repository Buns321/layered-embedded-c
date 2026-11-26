/**
 * @file bsp_def.h
 * @brief BSP 层统一状态码定义。
 * @details 定义板级支持包（BSP）所有公共接口共享的错误与状态枚举，
 *          位于硬件抽象最底层，供 BSP、Framework 与 User 层统一判定硬件操作结果。
 *
 * @defgroup BSP BSP 硬件抽象层
 * @{
 */

#ifndef LAYERED_CAR_BSP_DEF_H
#define LAYERED_CAR_BSP_DEF_H

/**
 * @brief BSP 公共接口统一返回状态码。
 */
typedef enum bsp_status_e {
  BSP_OK = 0U,         /**< 操作成功完成。 */
  BSP_ERR_INVALID_ARG, /**< 参数非法（空指针、越界地址、未对齐或不支持的配置）。 */
  BSP_ERR_BUSY,        /**< 外设或共享资源正忙（如 DMA 发送中、Flash 忙或重复注册）。 */
  BSP_ERR_TIMEOUT,     /**< 阻塞操作在指定超时时间内未完成。 */
  BSP_ERR_NO_MEM,      /**< 静态实例槽位或预分配静态资源不足。 */
  BSP_ERR_HAL,         /**< 底层 STM32 HAL 返回未分类硬件错误。 */
  BSP_ERR_UNSUPPORTED, /**< 当前芯片家族或配置不支持该操作。 */
} bsp_status_t;

#endif /* LAYERED_CAR_BSP_DEF_H */

/** @} */
