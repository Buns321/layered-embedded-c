/**
 * @file board_cfg.h
 * @brief F407 全向小车板级服务启动、只读配置与装配入口。
 * @details 集中隔离 STM32F407VET6 的 CubeMX 外设句柄、引脚映射与机械/控制参数，
 *          对应用层仅暴露无硬件细节的初始化与只读查询接口。
 *
 * @defgroup User_Board 板级配置与装配
 * @{
 */

#ifndef LAYERED_CAR_USER_BOARD_CFG_H
#define LAYERED_CAR_USER_BOARD_CFG_H

#include "car_omni.h"
#include <stdbool.h>

/**
 * @brief 初始化板级基础服务（DWT 周期计数与可选 RTT 日志后端）。
 * @return 初始化是否成功。
 * @retval true  基础服务初始化成功。
 * @pre 须在系统时钟配置（168 MHz）完成后调用，以便 DWT 周期换算正确。
 */
bool Board_BootServices(void);

/**
 * @brief 获取 F407 四轮全向小车只读硬件与控制配置。
 * @return 指向静态只读配置结构体 car_omni_cfg_t 的指针（永不为 NULL）。
 * @note 返回指针指向只读静态存储区，调用方不得修改其内容。
 */
const car_omni_cfg_t *Board_GetCarConfig(void);

/**
 * @brief 完成 F407 全向小车静态装配与外设初始化。
 * @return 成功时返回静态整车实例指针 car_omni_t*；失败时返回 NULL 并记录错误日志。
 * @retval NULL 参数校验或底层外设初始化失败（例如引脚/定时器句柄异常）。
 * @pre 须在 CubeMX 外设初始化及 Board_BootServices() 之后调用。
 * @note 内部使用静态对象 s_car，禁止多实例并发调用。
 */
car_omni_t *Board_InitCar(void);

#endif /* LAYERED_CAR_USER_BOARD_CFG_H */

/** @} */
