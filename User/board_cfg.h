/**
 * @file board_cfg.h
 * @brief F407 全向小车板级服务启动、只读配置与装配入口。
 */

#ifndef LAYERED_CAR_USER_BOARD_CFG_H
#define LAYERED_CAR_USER_BOARD_CFG_H

#include "car_omni.h"
#include <stdbool.h>

/** @brief 初始化 DWT 周期计数与可选 RTT 日志后端。 */
bool Board_BootServices(void);

/** @brief 获取 F407 四轮全向小车只读硬件与控制配置。 */
const car_omni_cfg_t *Board_GetCarConfig(void);

/** @brief 完成 F407 全向小车静态装配与外设初始化；失败返回 NULL。 */
car_omni_t *Board_InitCar(void);

#endif /* LAYERED_CAR_USER_BOARD_CFG_H */
