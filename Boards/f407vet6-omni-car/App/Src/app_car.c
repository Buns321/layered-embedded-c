/**
 * @file app_car.c
 * @brief F407 四轮全向小车 App 初始化和任务入口协调。
 * @ingroup app
 */

#include "app_car.h"
#include "app_car_internal.h"
#include "app_car_motion.h"
#include "app_car_runtime.h"
#include "system_log.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static app_car_state_t s_car;

/**
 * @brief 初始化小车 App 状态并保存逻辑服务依赖。
 * @param dependencies Core/Board 填写的依赖集合，不得为 NULL。
 * @retval true App 已初始化，或已有有效初始化状态。
 * @retval false 依赖缺失或必选服务为空。
 * @pre Core_Board_Init 已成功完成。
 * @pre RTT 日志后端已按构建选项完成初始化；首次初始化须在 RTOS 调度器启动前完成。
 * @post 成功时进入 RATE_CONFIG 状态，四轮保持禁用。
 * @note 不初始化具体硬件，不分配动态内存；重复调用保留已有状态。
 */
bool App_Car_Init(const app_car_dependencies_t *dependencies) {
  if (dependencies == NULL) {
    return false;
  }
  if (dependencies->chassis_h == NULL || dependencies->imu_h == NULL ||
      dependencies->input_h == NULL || dependencies->telemetry_h == NULL ||
      dependencies->motor_count == 0U || dependencies->motor_count > APP_CAR_MOTOR_CAPACITY) {
    return false;
  }
  for (uint8_t wheel = 0U; wheel < dependencies->motor_count; wheel++) {
    if (dependencies->motor_h[wheel] == NULL) {
      return false;
    }
  }
  if (s_car.initialized) {
    return true;
  }

  (void)memset(&s_car, 0, sizeof(s_car));
  s_car.dependencies_h = dependencies;
  App_Car_Motion_Init(&s_car.motion, dependencies);
  app_car_enter_state(&s_car, APP_CAR_BOOT_RATE_CONFIG, 0U);
  app_car_set_heartbeat(&s_car, 0U);
  s_car.initialized = true;
  SYS_LOG_INFO("CAR", "app initialized, motors disabled");
  return true;
}

/**
 * @brief 推进 IMU、输入设备和上电归零状态机。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 UARTTask 调用，Core_Board_Init 和 App_Car_Init 已成功返回。
 * @post 已处理 IMU 帧、输入帧及启动状态机。
 * @note 不分配内存，不阻塞 RTOS 队列。
 */
void App_Car_ProcessInputs(uint32_t now_ms) {
  if (!s_car.initialized) {
    return;
  }

  app_car_process_inputs(&s_car, now_ms);
}

/**
 * @brief 运行一次 100 Hz 底盘与四轮闭环控制。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 ControlTask 调用。
 * @post 已推进安全判断、输入映射、底盘解算和四轮闭环。
 * @note 不分配内存，不阻塞等待。
 */
void App_Car_ProcessControl(uint32_t now_ms) {
  if (!s_car.initialized) {
    return;
  }

  app_car_process_control(&s_car, now_ms);
}

/**
 * @brief 运行一次监控、遥测和状态日志步骤。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @pre 只允许 MonitorTask 调用。
 * @post 已检查控制心跳并按周期提交遥测和日志。
 * @note 不分配内存，不等待 DMA。
 */
void App_Car_ProcessMonitor(uint32_t now_ms) {
  if (!s_car.initialized) {
    return;
  }

  app_car_process_safety(&s_car, now_ms);
  app_car_process_telemetry(&s_car, now_ms);
  app_car_process_log(&s_car, now_ms);
}
