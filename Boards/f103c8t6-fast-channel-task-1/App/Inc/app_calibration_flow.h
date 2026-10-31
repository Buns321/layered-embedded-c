/**
 * @file app_calibration_flow.h
 * @brief 快速通道任务一初始姿态、陀螺零偏和六面标定状态机。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_CALIBRATION_FLOW_H
#define LAYERED_EMBEDDED_C_APP_CALIBRATION_FLOW_H

#include "app_calibration.h"
#include "app_calibration_record.h"
#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 标定会话状态。
 */
typedef enum app_fast_channel_state_e {
  APP_FAST_CHANNEL_STATE_UNINITIALIZED = 0U,
  APP_FAST_CHANNEL_STATE_INITIAL_POSE_CHECK,
  APP_FAST_CHANNEL_STATE_GYRO_CALIBRATION,
  APP_FAST_CHANNEL_STATE_WAIT_FACE,
  APP_FAST_CHANNEL_STATE_STABILIZE_FACE,
  APP_FAST_CHANNEL_STATE_SAMPLE_FACE,
  APP_FAST_CHANNEL_STATE_MAIN,
  APP_FAST_CHANNEL_STATE_ERROR,
} app_fast_channel_state_t;

/**
 * @brief 标定流程事件位。
 */
typedef enum app_calibration_event_e {
  APP_CALIBRATION_EVENT_NONE = 0U,
  APP_CALIBRATION_EVENT_GYRO_READY = 1U << 0,
  APP_CALIBRATION_EVENT_FACE_SAVED = 1U << 1,
  APP_CALIBRATION_EVENT_FACE_REJECTED = 1U << 2,
  APP_CALIBRATION_EVENT_FACE_COMMIT_FAILED = 1U << 3,
  APP_CALIBRATION_EVENT_MAIN_ENTERED = 1U << 4,
} app_calibration_event_t;

/**
 * @brief 标定流程私有状态。
 */
typedef struct app_calibration_flow_s {
  app_calibration_record_store_t *record_store_h;
  app_fast_channel_state_t state;
  app_calibration_record_t working_record;
  float face_values_mps2[6u][APP_CALIBRATION_AXIS_COUNT];
  float face_sample_sum_mps2[APP_CALIBRATION_AXIS_COUNT];
  float gyro_sum_radps[APP_CALIBRATION_AXIS_COUNT];
  float gyro_sum_sq_radps2[APP_CALIBRATION_AXIS_COUNT];
  uint32_t face_sample_count;
  uint32_t candidate_face;
  uint32_t face_mask;
  uint32_t completed_axis_mask;
  uint32_t state_started_ms;
  uint32_t stable_started_ms;
  uint32_t face_deadline_ms;
  uint32_t inverted_since_ms;
  uint32_t gyro_sample_count;
  bool inverted_tracking;
  bool main_announced;
  bool started;
  bool initialized;
} app_calibration_flow_t;

/**
 * @brief 初始化标定流程。
 * @param flow_h 待初始化流程，不得为 NULL。
 * @param record_store_h 已初始化校准记录存储，不得为 NULL。
 * @retval true 流程进入 INITIAL_POSE_CHECK。
 * @retval false 任一指针为空。
 * @pre 记录存储已成功初始化，调用发生在调度器启动前。
 * @post working record 复制最近活动记录，全部采样和计时状态清零。
 * @note 不访问 Flash、不分配内存。
 */
bool App_CalibrationFlow_Init(
  app_calibration_flow_t *flow_h,
  app_calibration_record_store_t *record_store_h
);

/**
 * @brief 推进一次初始姿态、陀螺零偏或六面标定状态机。
 * @param flow_h 已初始化流程，不得为 NULL。
 * @param raw_valid 原始样本是否有有效帧。
 * @param raw_sample 原始样本；raw_valid 为 true 时不得为 NULL。
 * @param calibrated_valid 校正样本是否有有效帧。
 * @param calibrated_sample 校正样本；calibrated_valid 为 true 时不得为 NULL。
 * @param now_ms 当前单调毫秒时间戳，单位 ms。
 * @return 本次产生的事件位组合，可为 NONE。
 * @pre 只由单一 IMU 任务调用，且流程已成功初始化。
 * @post 状态、采样累计、面掩码和工作记录按样本推进；达到条件时可能提交 Flash。
 * @note 可能执行阻塞 Flash 操作；不分配内存，不使用 GPIO 或控制 EKF。
 */
uint32_t App_CalibrationFlow_Process(
  app_calibration_flow_t *flow_h,
  bool raw_valid,
  const imu_sample_t *raw_sample,
  bool calibrated_valid,
  const imu_sample_t *calibrated_sample,
  uint32_t now_ms
);

/**
 * @brief 取得当前标定状态。
 * @param flow_h 流程对象；可为 NULL。
 * @return 当前状态；NULL 时返回 UNINITIALIZED。
 * @pre 无。
 * @post 不修改流程。
 * @note 结果供快照和 LED 周期选择使用。
 */
app_fast_channel_state_t App_CalibrationFlow_GetState(
  const app_calibration_flow_t *flow_h
);

/**
 * @brief 取得已接受安装面位掩码。
 * @param flow_h 流程对象；可为 NULL。
 * @return 已接受面位掩码；NULL 时返回 0。
 * @pre 无。
 * @post 不修改流程。
 * @note 位定义来自 app_calibration.h。
 */
uint32_t App_CalibrationFlow_GetFaceMask(
  const app_calibration_flow_t *flow_h
);

/**
 * @brief 取得已完成正负样本对的轴位掩码。
 * @param flow_h 流程对象；可为 NULL。
 * @return 已完成轴位掩码；NULL 时返回 0。
 * @pre 无。
 * @post 不修改流程。
 * @note 位 0..2 分别对应 X/Y/Z。
 */
uint32_t App_CalibrationFlow_GetCompletedAxisMask(
  const app_calibration_flow_t *flow_h
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_CALIBRATION_FLOW_H
