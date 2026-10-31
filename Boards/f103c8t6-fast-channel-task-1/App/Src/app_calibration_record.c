/**
 * @file app_calibration_record.c
 * @brief 快速通道任务一 IMU 校准记录持久化实现。
 * @ingroup app
 */

#include "app_calibration_record.h"
#include "app_calibration.h"
#include "app_fast_channel_config.h"
#include "system_log.h"
#include "system_math.h"
#include <assert.h>
#include <stddef.h>
#include <string.h>

#define APP_CALIBRATION_RAD_TO_DEG 57.295779513082320876f

_Static_assert(
  sizeof(app_calibration_record_t) <= SYSTEM_RECORD_STORE_PAYLOAD_CAPACITY,
  "calibration record exceeds record store capacity"
);

/**
 * @brief 校验从 Flash 读取的记录版本、掩码和数值范围。
 * @param record 待校验记录，不得为 NULL。
 * @param version 记录版本。
 * @param payload_size 负载长度，单位 byte。
 * @return 版本、长度、掩码、有限值和比例范围全部合法时返回 true。
 * @pre record 非 NULL。
 * @post 不修改记录或外部状态。
 * @note 内部辅助函数，不访问 Flash、不分配内存。
 */
static bool app_calibrationrecord_isvalid(
  const app_calibration_record_t *record,
  uint16_t version,
  uint32_t payload_size
) {
  if (
    record == NULL ||
    version != APP_CALIBRATION_RECORD_VERSION ||
    payload_size != sizeof(*record) ||
    (record->face_mask & ~((uint32_t)APP_FACE_ALL)) != 0u ||
    (
      record->completed_axis_mask &
      ~((uint32_t)APP_CALIBRATION_AXIS_COMPLETE_MASK)
    ) != 0u
  ) {
    return false;
  }

  for (uint8_t axis = 0u; axis < APP_CALIBRATION_AXIS_COUNT; axis++) {
    if (
      !SYS_ISFINITE(record->gyro_bias_radps[axis]) ||
      !SYS_ISFINITE(record->accel_offset_mps2[axis]) ||
      !SYS_ISFINITE(record->accel_scale_ratio[axis]) ||
      record->accel_scale_ratio[axis] < APP_ACCEL_SCALE_MIN ||
      record->accel_scale_ratio[axis] > APP_ACCEL_SCALE_MAX
    ) {
      return false;
    }
  }
  return true;
}

/**
 * @brief 把记录初始化为零偏 0、加速度比例 1 的安全默认值。
 * @param record 输出记录，不得为 NULL。
 * @pre record 可写。
 * @post 全部字段清零，三轴 accel_scale_ratio 均为 1。
 * @note 不访问 Flash、不分配内存。
 */
void App_CalibrationRecord_InitDefaults(
  app_calibration_record_t *record
) {
  assert(record);
  memset(record, 0, sizeof(*record));
  for (uint8_t axis = 0u; axis < APP_CALIBRATION_AXIS_COUNT; axis++) {
    record->accel_scale_ratio[axis] = 1.0f;
  }
}

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
) {
  if (store_h == NULL || filter_h == NULL) {
    return false;
  }

  memset(store_h, 0, sizeof(*store_h));
  store_h->filter_h = filter_h;
  App_CalibrationRecord_InitDefaults(&store_h->active_record);
  if (!App_CalibrationRecord_Apply(store_h, &store_h->active_record)) {
    return false;
  }
  store_h->initialized = true;
  return true;
}

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
) {
  if (store_h == NULL || !store_h->initialized) {
    return false;
  }

  system_record_store_status_t store_status = Sys_RecordStore_Init(
    &store_h->record_store,
    PLATFORM_FLASH_1,
    APP_CALIBRATION_FLASH_ADDRESS,
    APP_CALIBRATION_FLASH_PAGE_SIZE,
    APP_CALIBRATION_RECORD_MAGIC
  );
  if (store_status != SYSTEM_RECORD_STORE_OK) {
    store_h->flash_write_failed = true;
    SYS_LOG_ERROR("FLASH", "record store init failed");
    return false;
  }

  app_calibration_record_t record;
  uint16_t version = 0u;
  uint32_t payload_size = 0u;
  store_status = Sys_RecordStore_LoadLatest(
    &store_h->record_store,
    &version,
    &record,
    sizeof(record),
    &payload_size
  );
  if (
    store_status != SYSTEM_RECORD_STORE_OK ||
    !app_calibrationrecord_isvalid(&record, version, payload_size)
  ) {
    return false;
  }

  if (!App_CalibrationRecord_Apply(store_h, &record)) {
    store_h->flash_write_failed = true;
    return false;
  }

  store_h->active_record = record;
  store_h->flash_record_valid = true;
  store_h->flash_write_failed = false;
  return true;
}

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
) {
  if (
    store_h == NULL ||
    store_h->filter_h == NULL ||
    record == NULL
  ) {
    return false;
  }

  filter_imu_calibration_config_t config = {0};
  for (uint8_t axis = 0u; axis < APP_CALIBRATION_AXIS_COUNT; axis++) {
    config.accel_offset_mps2[axis] = record->accel_offset_mps2[axis];
    config.accel_scale[axis] = record->accel_scale_ratio[axis];
    config.gyro_bias_dps[axis] =
      record->gyro_bias_radps[axis] * APP_CALIBRATION_RAD_TO_DEG;
  }
  bool applied = Filter_ImuCalibration_SetConfig(
    store_h->filter_h,
    &config
  );
  if (!applied) {
    store_h->flash_write_failed = true;
  }
  return applied;
}

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
) {
  if (
    store_h == NULL ||
    !store_h->initialized ||
    record == NULL
  ) {
    return false;
  }

  if (
    Sys_RecordStore_Append(
      &store_h->record_store,
      APP_CALIBRATION_RECORD_VERSION,
      record,
      sizeof(*record)
    ) != SYSTEM_RECORD_STORE_OK
  ) {
    store_h->flash_write_failed = true;
    return false;
  }

  if (!App_CalibrationRecord_Apply(store_h, record)) {
    store_h->flash_write_failed = true;
    store_h->flash_record_valid = true;
    return false;
  }

  store_h->active_record = *record;
  store_h->flash_record_valid = true;
  store_h->flash_write_failed = false;
  return true;
}

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
) {
  assert(store_h);
  return &store_h->active_record;
}

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
) {
  return store_h != NULL && store_h->flash_record_valid;
}

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
) {
  return store_h != NULL && store_h->flash_write_failed;
}
