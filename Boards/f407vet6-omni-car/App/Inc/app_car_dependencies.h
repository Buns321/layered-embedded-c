/**
 * @file app_car_dependencies.h
 * @brief F407 四轮小车 App 所需的逻辑服务依赖。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_CAR_DEPENDENCIES_H
#define LAYERED_EMBEDDED_C_APP_CAR_DEPENDENCIES_H

#include "chassis_base.h"
#include "imu_base.h"
#include "joycon_base.h"
#include "motor_base.h"
#include "telemetry_base.h"
#include <stdint.h>

/** @brief 小车 App 支持的最大电机数量。 */
#define APP_CAR_MOTOR_CAPACITY 4u

/**
 * @brief 小车 App 的静态依赖集合。
 * @note 所有指针由 Core/Board 创建并保证生命周期覆盖整个固件运行期。
 * @note App 只读使用这些依赖，不拥有、释放或替换其中的对象。
 */
typedef struct app_car_dependencies_s {
  /** @brief 底盘领域接口。 */
  chassis_base_t *chassis_h;
  /** @brief 闭环电机基类视图。 */
  motor_base_t *motor_h[APP_CAR_MOTOR_CAPACITY];
  /** @brief 有效电机数量，范围 1..APP_CAR_MOTOR_CAPACITY。 */
  uint8_t motor_count;
  /** @brief 统一 IMU 服务。 */
  imu_base_t *imu_h;
  /** @brief JoyCon 输入服务。 */
  joycon_base_t *input_h;
  /** @brief 遥测发送服务。 */
  telemetry_base_t *telemetry_h;
} app_car_dependencies_t;

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_CAR_DEPENDENCIES_H
