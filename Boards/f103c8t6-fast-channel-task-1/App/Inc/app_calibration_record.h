/**
 * @file app_calibration_record.h
 * @brief 快速通道任务一 IMU 校准记录持久化。
 * @addtogroup app
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_APP_CALIBRATION_RECORD_H
#define LAYERED_EMBEDDED_C_APP_CALIBRATION_RECORD_H

#include "filter_imu_calibration.h"
#include "system_record_store.h"
#include <stdbool.h>
#include <stdint.h>

#define APP_CALIBRATION_AXIS_COUNT 3u
#define APP_CALIBRATION_AXIS_COMPLETE_MASK 0x07u

/**
 * @brief 持久化的 IMU 校准负载。
 */
typedef struct app_calibration_record_s {
  /** @brief 已完成安装面位掩码。 */
  uint32_t face_mask;
  /** @brief 已完成正负样本对的轴位掩码。 */
  uint32_t completed_axis_mask;
  /** @brief 三轴陀螺零偏，单位 rad/s。 */
  float gyro_bias_radps[APP_CALIBRATION_AXIS_COUNT];
  /** @brief 三轴加速度零偏，单位 m/s²。 */
  float accel_offset_mps2[APP_CALIBRATION_AXIS_COUNT];
  /** @brief 三轴加速度比例系数。 */
  float accel_scale_ratio[APP_CALIBRATION_AXIS_COUNT];
} app_calibration_record_t;

/**
 * @brief 校准记录存储和活动配置。
 * @note record_store 和 active_record 归本对象所有；filter_h 为非拥有依赖。
 */
typedef struct app_calibration_record_store_s {
  filter_imu_calibration_t *filter_h;
  system_record_store_t record_store;
  app_calibration_record_t active_record;
  bool flash_record_valid;
  bool flash_write_failed;
  bool initialized;
} app_calibration_record_store_t;

/**
 * @brief 把记录初始化为零偏 0、加速度比例 1 的安全默认值。
 * @param record 输出记录，不得为 NULL。
 * @pre record 可写。
 * @post 全部字段清零，三轴 accel_scale_ratio 均为 1。
 * @note 不访问 Flash、不分配内存。
 */
void App_CalibrationRecord_InitDefaults(
  app_calibration_record_t *record
);

/**
 * @brief 初始化记录存储并应用默认记录。
 * @param store_h 待初始化记录存储，不得为 NULL。
 * @param filter_h 已初始化的六轴校正滤波器，不得为 NULL。
 * @retval true 默认记录已成功应用。
 * @retval false 指针为空或默认配置被滤波器拒绝。
 * @pre 只在初始化阶段单线程调用。
 * @post 成功时 active_record 为默认值、Flash 状态清零、initialized 为 true。
 * @note 不访问 Flash、不分配内存。
 */
bool App_CalibrationRecord_Init(
  app_calibration_record_store_t *store_h,
  filter_imu_calibration_t *filter_h
);

/**
 * @brief 初始化双页 Flash 记录存储并加载最新有效记录。
 * @param store_h 已初始化记录存储，不得为 NULL。
 * @retval true 已加载并应用一条 CRC 和内容校验均有效的记录。
 * @retval false 没有有效记录、Flash 初始化失败或应用失败。
 * @pre store_h 已由 App_CalibrationRecord_Init 成功初始化。
 * @post 成功时 active_record 更新；失败时保留默认或已有活动记录，并锁存失败标志。
 * @note 可能同步扫描和擦除 Flash；不分配内存，只允许初始化阶段调用。
 */
bool App_CalibrationRecord_Load(
  app_calibration_record_store_t *store_h
);

/**
 * @brief 把记录应用到六轴校正滤波器。
 * @param store_h 已初始化记录存储，不得为 NULL。
 * @param record 已验证记录，不得为 NULL。
 * @retval true 记录已转换为滤波器配置并成功提交。
 * @retval false 指针为空或滤波器拒绝配置。
 * @pre record 中的物理量和比例系数已通过有限性和范围校验。
 * @post 成功时滤波器后续帧使用该记录；失败时滤波器配置保持不变。
 * @note 不写 Flash、不分配内存。
 */
bool App_CalibrationRecord_Apply(
  app_calibration_record_store_t *store_h,
  const app_calibration_record_t *record
);

/**
 * @brief 追加工作记录并在回读成功后更新活动配置。
 * @param store_h 已初始化记录存储，不得为 NULL。
 * @param record 待持久化工作记录，不得为 NULL。
 * @retval true 记录已追加、回读校验通过并应用到滤波器。
 * @retval false Flash 写入或应用失败；active_record 保持旧值。
 * @pre 单写者调用，且在任务上下文执行。
 * @post 成功时 active_record 更新并清除 flash_write_failed；失败时置位该标志。
 * @note 可能阻塞执行 Flash 擦写；不分配内存。
 */
bool App_CalibrationRecord_Commit(
  app_calibration_record_store_t *store_h,
  const app_calibration_record_t *record
);

/**
 * @brief 取得最近一次有效活动记录。
 * @param store_h 已初始化记录存储，不得为 NULL。
 * @return 指向内部活动记录的只读指针；生命周期跟随 store_h。
 * @pre store_h 已成功初始化。
 * @post 不修改记录或 Flash 状态。
 * @note 返回指针不得由调用方释放；并发写入时调用方须串行化。
 */
const app_calibration_record_t *App_CalibrationRecord_GetActive(
  const app_calibration_record_store_t *store_h
);

/**
 * @brief 查询是否从 Flash 成功加载过有效记录。
 * @param store_h 记录存储；可为 NULL。
 * @return 有效记录标志；NULL 时返回 false。
 * @pre 无。
 * @post 不修改状态。
 * @note 该值表示初始化加载结果，不表示最近一次提交是否成功。
 */
bool App_CalibrationRecord_HasFlashRecord(
  const app_calibration_record_store_t *store_h
);

/**
 * @brief 查询本次运行是否发生过 Flash 写入或应用失败。
 * @param store_h 记录存储；可为 NULL。
 * @return 失败标志；NULL 时返回 false。
 * @pre 无。
 * @post 不修改状态。
 * @note 标志只在成功提交或重新初始化后清除。
 */
bool App_CalibrationRecord_HasFlashFailure(
  const app_calibration_record_store_t *store_h
);

/** @} */

#endif //LAYERED_EMBEDDED_C_APP_CALIBRATION_RECORD_H
