/**
 * @file app_car_motion.c
 * @brief 小车底盘 twist 和电机闭环运动执行。
 * @ingroup app
 */

#include "app_car_motion.h"
#include <assert.h>
#include <stddef.h>

/**
 * @brief 从 App 依赖初始化 Motion 服务。
 * @param motion_h 待初始化 Motion 对象，不得为 NULL。
 * @param dependencies 已初始化的 App 依赖，不得为 NULL。
 * @post Motion 保存底盘和电机非 owning 指针。
 * @note 只允许初始化阶段调用。
 */
void App_Car_Motion_Init(
  app_car_motion_t *motion_h,
  const app_car_dependencies_t *dependencies
) {
  assert(motion_h && dependencies);
  assert(
    dependencies->chassis_h &&
    dependencies->motor_count > 0U &&
    dependencies->motor_count <= APP_CAR_MOTOR_CAPACITY
  );

  motion_h->chassis_h = dependencies->chassis_h;
  motion_h->motor_count = dependencies->motor_count;
  for (uint8_t wheel = 0U; wheel < motion_h->motor_count; wheel++) {
    assert(dependencies->motor_h[wheel]);
    motion_h->motor_h[wheel] = dependencies->motor_h[wheel];
  }
}

/**
 * @brief 保存车体系速度命令。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @param twist 车体系速度命令，不得为 NULL。
 * @post 命令已提交给底盘接口。
 * @note 不推进电机闭环。
 */
void App_Car_Motion_SetTwist(
  app_car_motion_t *motion_h,
  const chassis_twist_t *twist
) {
  assert(motion_h && twist);
  Chassis_Base_SetMotion(motion_h->chassis_h, twist);
}

/**
 * @brief 推进一次底盘解算和全部电机闭环。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @param now_ms 当前时间，单位 ms。
 * @return 本次更新结果。
 * @retval APP_CAR_MOTION_OK 底盘和全部电机更新成功。
 * @retval APP_CAR_MOTION_MOTORS_DISABLED 电机未全部使能，未执行电机更新。
 * @retval APP_CAR_MOTION_CONTROLLER_ERROR 任一控制器返回错误。
 * @pre 只允许控制任务调用。
 * @post 底盘目标已解算；电机状态允许时全部闭环已推进。
 * @note 遇到首个控制器错误后停止后续电机更新。
 */
app_car_motion_status_t App_Car_Motion_Update(
  app_car_motion_t *motion_h,
  uint32_t now_ms
) {
  assert(motion_h);

  Chassis_Base_Update(motion_h->chassis_h, now_ms);

  if (!App_Car_Motion_MotorsAreEnabled(motion_h)) {
    return APP_CAR_MOTION_MOTORS_DISABLED;
  }

  for (uint8_t wheel = 0U; wheel < motion_h->motor_count; wheel++) {
    motor_base_t *motor_h = motion_h->motor_h[wheel];
    Dev_Motor_Base_Update(motor_h);
    if (
      Dev_Motor_Base_ReadControllerStatus(motor_h) ==
      CONTROLLER_STATUS_ERROR
    ) {
      return APP_CAR_MOTION_CONTROLLER_ERROR;
    }
  }

  return APP_CAR_MOTION_OK;
}

/**
 * @brief 使能全部电机并回读状态。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @retval true 全部电机均为使能状态。
 * @retval false 任一电机未进入使能状态。
 * @post 已向全部电机发出使能请求。
 * @note 只允许控制任务调用。
 */
bool App_Car_Motion_EnableMotors(app_car_motion_t *motion_h) {
  assert(motion_h);

  for (uint8_t wheel = 0U; wheel < motion_h->motor_count; wheel++) {
    Dev_Motor_Base_Enable(motion_h->motor_h[wheel]);
  }

  return App_Car_Motion_MotorsAreEnabled(motion_h);
}

/**
 * @brief 检查全部电机是否使能。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @retval true 全部电机均为 ENABLE。
 * @retval false 任一电机不是 ENABLE。
 * @post 不修改状态。
 * @note 只读取电机状态。
 */
bool App_Car_Motion_MotorsAreEnabled(
  const app_car_motion_t *motion_h
) {
  assert(motion_h);

  for (uint8_t wheel = 0U; wheel < motion_h->motor_count; wheel++) {
    if (Dev_Motor_Base_ReadStatus(motion_h->motor_h[wheel]) != DEV_MOTOR_ENABLE) {
      return false;
    }
  }
  return true;
}

/**
 * @brief 请求底盘常规停止。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @post 底盘已请求清零目标和制动。
 * @note 只允许控制任务调用。
 */
void App_Car_Motion_Stop(app_car_motion_t *motion_h) {
  assert(motion_h);
  Chassis_Base_Stop(motion_h->chassis_h);
}

/**
 * @brief 请求底盘急停。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @post 底盘已请求禁用执行器输出。
 * @note 只允许控制或监控任务调用。
 */
void App_Car_Motion_EStop(app_car_motion_t *motion_h) {
  assert(motion_h);
  Chassis_Base_EStop(motion_h->chassis_h);
}

/**
 * @brief 读取电机目标速度。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @param wheel 电机下标，必须小于 motor_count。
 * @return 目标速度，单位 m/s。
 * @post 不修改 Motion 状态。
 * @note 范围由底盘电机配置决定。
 */
float App_Car_Motion_ReadTargetSpeed(
  const app_car_motion_t *motion_h,
  uint8_t wheel
) {
  assert(motion_h && wheel < motion_h->motor_count);
  return Dev_Motor_Base_ReadTargetSpeed(motion_h->motor_h[wheel]);
}

/**
 * @brief 读取电机当前速度。
 * @param motion_h 已初始化 Motion 对象，不得为 NULL。
 * @param wheel 电机下标，必须小于 motor_count。
 * @return 当前速度，单位 m/s。
 * @post 不修改 Motion 状态。
 * @note 只反映最近一次闭环更新结果。
 */
float App_Car_Motion_ReadCurrentSpeed(
  const app_car_motion_t *motion_h,
  uint8_t wheel
) {
  assert(motion_h && wheel < motion_h->motor_count);
  return Dev_Motor_Base_ReadCurrentSpeed(motion_h->motor_h[wheel]);
}
