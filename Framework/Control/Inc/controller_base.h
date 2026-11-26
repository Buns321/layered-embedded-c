/**
 * @file controller_base.h
 * @brief 轮级 SISO 控制律抽象接口与多态基类。
 * @addtogroup control
 * @{
 */

#ifndef LAYERED_CAR_CONTROLLER_BASE_H
#define LAYERED_CAR_CONTROLLER_BASE_H

#include <stdbool.h>
#include <stdint.h>

typedef struct controller_base_s controller_base_t;

/**
 * @brief 控制律单步执行状态。
 */
typedef enum controller_status_e {
  CONTROLLER_STATUS_OK = 0U,
  CONTROLLER_STATUS_SATURATED,
  CONTROLLER_STATUS_ERROR
} controller_status_t;

/**
 * @brief 控制律虚表（target/measure 轮速单位 m/s，dt_s 单位 s，out 为 duty）。
 */
typedef struct controller_ops_s {
  controller_status_t (*update)(controller_base_t *controller_h, float target, float measure,
                                float dt_s, float *out);
  void (*reset)(controller_base_t *controller_h);
} controller_ops_t;

/**
 * @brief 控制律多态基类；须作为具体控制器结构体首成员。
 */
typedef struct controller_base_s {
  const controller_ops_t *ops;
  const char *name;
} controller_base_t;

/**
 * @brief 绑定控制律名称与静态虚表（仅初始化阶段调用）。
 */
void Ctrl_Base_Init(controller_base_t *controller_h, const char *name, const controller_ops_t *ops);

/**
 * @brief 运行一次控制律计算；非法指针或 dt_s <= 0 时清零 *out 并返回 ERROR。
 */
controller_status_t Ctrl_Base_Update(controller_base_t *controller_h, float target, float measure,
                                     float dt_s, float *out);

/**
 * @brief 复位控制律内部运行状态（保留配置参数与绑定关系）。
 */
void Ctrl_Base_Reset(controller_base_t *controller_h);

/** @} */

#endif /* LAYERED_CAR_CONTROLLER_BASE_H */
