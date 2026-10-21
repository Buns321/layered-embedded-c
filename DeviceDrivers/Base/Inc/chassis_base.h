//
// Created by Buns_ on 2026/10/20.
//

/**
 * @file chassis_base.h
 * @brief 底盘生命周期抽象：update / stop / estop。
 * @addtogroup chassis
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CHASSIS_BASE_H
#define LAYERED_EMBEDDED_C_CHASSIS_BASE_H

#include <stdint.h>

typedef struct chassis_base chassis_base_t;

/**
 * @brief 底盘具体实现的操作表。
 * @note update 只负责解算与下发目标；电机闭环由调用方逐轮运行。
 */
typedef struct chassis_ops {
  void (*update)(chassis_base_t *this, uint32_t now_ms);
  void (*stop)  (chassis_base_t *this);
  void (*estop) (chassis_base_t *this);
} chassis_ops_t;

/**
 * @brief 底盘抽象对象。
 * @note 基类只承载生命周期；命令接口由具体底盘类型提供。
 */
typedef struct chassis_base {
  const chassis_ops_t *ops;
  const char          *name;
} chassis_base_t;

/** @brief 初始化底盘抽象层并绑定操作表。 */
void Chassis_Base_Init(chassis_base_t *this, const char *name, const chassis_ops_t *ops);
/**
 * @brief 运行一次底盘解算与目标下发。
 * @param this 底盘对象。
 * @param now_ms 当前单调毫秒时间戳，用于 IMU 数据新鲜度判断。
 */
void Chassis_Base_Update(chassis_base_t *this, uint32_t now_ms);
/**
 * @brief 常规停机：清零目标并制动。
 * @note 制动状态保持到电机被显式重新使能。
 */
void Chassis_Base_Stop(chassis_base_t *this);
/** @brief 紧急停机：禁用执行器输出。 */
void Chassis_Base_EStop(chassis_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_CHASSIS_BASE_H
