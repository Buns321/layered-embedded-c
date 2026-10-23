//
// 由 Buns_ 创建于 2026/10/17。
//

/**
 * @file controller_base.h
 * @brief 轮级 SISO 控制律接口。
 * @addtogroup controller
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_CONTROLLER_BASE_H
#define LAYERED_EMBEDDED_C_CONTROLLER_BASE_H

#include <stdint.h>

typedef struct controller_base controller_base_t;

/**
 * @brief 控制律执行状态。
 */
typedef enum {
  CONTROLLER_STATUS_OK = 0U,
  CONTROLLER_STATUS_SATURATED,
  CONTROLLER_STATUS_ERROR,
} controller_status_t;

/**
 * @brief 控制律操作表。
 * @note 契约：target / measure 是当前槽位物理量（轮速环为 m/s）；
 *       dt 单位 s，必须大于 0；out 是执行器量（轮速环为 duty）。
 */
typedef struct controller_ops {
  controller_status_t (*update)(
    controller_base_t *this,
    float target,
    float measure,
    float dt,
    float *out
  );
  void (*reset)(controller_base_t *this);
} controller_ops_t;

/**
 * @brief 控制律抽象对象。
 */
typedef struct controller_base {
  const controller_ops_t *ops;
  const char             *name;
} controller_base_t;

/** @brief 初始化控制律抽象层并绑定操作表。 */
void Ctrl_Base_Init(controller_base_t *this, const char *name, const controller_ops_t *ops);
/**
 * @brief 运行一次控制律。
 * @param this 控制律对象。
 * @param target 参考值。
 * @param measure 实测值。
 * @param dt 控制周期（s），必须有限且大于 0。
 * @param out 输出指针，不能为空。
 * @retval CONTROLLER_STATUS_OK 正常。
 * @retval CONTROLLER_STATUS_SATURATED 输出饱和。
 * @retval CONTROLLER_STATUS_ERROR 输入/输出错误。
 */
controller_status_t Ctrl_Base_Update(
  controller_base_t *this,
  float target,
  float measure,
  float dt,
  float *out
);
/** @brief 复位控制律内部状态。 */
void Ctrl_Base_Reset(controller_base_t *this);

/** @} */

#endif //LAYERED_EMBEDDED_C_CONTROLLER_BASE_H
