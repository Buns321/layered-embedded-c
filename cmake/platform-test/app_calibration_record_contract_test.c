/**
 * @file app_calibration_record_contract_test.c
 * @brief 快速通道任务一校准记录持久化契约测试。
 * @ingroup tests
 */

#include "app_calibration_record.h"
#include "app_calibration.h"
#include "app_fast_channel_config.h"
#include "system_log.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static bool s_set_config_result = true;
static uint32_t s_set_config_calls;
static bool s_append_result = true;
static uint32_t s_append_calls;
static bool s_store_init_result = true;
static bool s_load_result = true;
static app_calibration_record_t s_loaded_record;
volatile uint32_t g_sys_log_fast_mask;

/**
 * @brief 记录滤波器配置提交调用的测试替身。
 * @param filter_h 滤波器对象；测试忽略。
 * @param config 非空配置；测试不访问。
 * @return s_set_config_result。
 * @pre config 非空。
 * @post s_set_config_calls 加一。
 * @note 不访问真实滤波器。
 */
bool Filter_ImuCalibration_SetConfig(
  filter_imu_calibration_t *filter_h,
  const filter_imu_calibration_config_t *config
) {
  (void)filter_h;
  assert(config);
  s_set_config_calls++;
  return s_set_config_result;
}

/**
 * @brief 记录存储初始化测试替身。
 * @param store_h 存储对象；测试忽略。
 * @param flash Flash 实例；测试忽略。
 * @param first_bank_address 页地址；测试忽略。
 * @param page_size 页大小；测试忽略。
 * @param magic 记录 Magic；测试忽略。
 * @return s_store_init_result 对应状态。
 * @pre 无。
 * @post 不修改存储对象。
 * @note 不访问真实 Flash。
 */
system_record_store_status_t Sys_RecordStore_Init(
  system_record_store_t *store_h,
  platform_flash_instance_t flash,
  uint32_t first_bank_address,
  uint32_t page_size,
  uint32_t magic
) {
  (void)store_h;
  (void)flash;
  (void)first_bank_address;
  (void)page_size;
  (void)magic;
  return s_store_init_result
    ? SYSTEM_RECORD_STORE_OK
    : SYSTEM_RECORD_STORE_FLASH_ERROR;
}

/**
 * @brief 记录加载测试替身。
 * @param store_h 存储对象；测试忽略。
 * @param version 输出版本，不得为 NULL。
 * @param payload 输出负载，不得为 NULL。
 * @param payload_capacity 负载容量，单位 byte。
 * @param payload_size 输出负载长度，不得为 NULL。
 * @return s_load_result 对应状态。
 * @pre 输出指针非空且容量足够。
 * @post 成功时复制 s_loaded_record 并写版本和长度。
 * @note 不访问真实 Flash。
 */
system_record_store_status_t Sys_RecordStore_LoadLatest(
  const system_record_store_t *store_h,
  uint16_t *version,
  void *payload,
  uint32_t payload_capacity,
  uint32_t *payload_size
) {
  (void)store_h;
  assert(version && payload && payload_size);
  if (!s_load_result || payload_capacity < sizeof(s_loaded_record)) {
    return SYSTEM_RECORD_STORE_EMPTY;
  }
  *version = APP_CALIBRATION_RECORD_VERSION;
  memcpy(payload, &s_loaded_record, sizeof(s_loaded_record));
  *payload_size = sizeof(s_loaded_record);
  return SYSTEM_RECORD_STORE_OK;
}

/**
 * @brief 记录追加测试替身。
 * @param store_h 存储对象；测试忽略。
 * @param version 记录版本；测试忽略。
 * @param payload 负载；测试忽略。
 * @param payload_size 负载长度，单位 byte；测试忽略。
 * @return s_append_result 对应状态。
 * @pre 无。
 * @post s_append_calls 加一。
 * @note 不访问真实 Flash。
 */
system_record_store_status_t Sys_RecordStore_Append(
  system_record_store_t *store_h,
  uint16_t version,
  const void *payload,
  uint32_t payload_size
) {
  (void)store_h;
  (void)version;
  (void)payload;
  (void)payload_size;
  s_append_calls++;
  return s_append_result
    ? SYSTEM_RECORD_STORE_OK
    : SYSTEM_RECORD_STORE_FLASH_ERROR;
}

/**
 * @brief 日志写入测试替身。
 * @param level 日志等级；测试忽略。
 * @param tag 标签；测试忽略。
 * @param format 格式；测试忽略。
 * @param ... 可变参数；测试忽略。
 * @post 函数体为空。
 * @note 不输出日志。
 */
void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
) {
  (void)level;
  (void)tag;
  (void)format;
}

/**
 * @brief 构造一条有效记录。
 * @param record 输出记录，不得为 NULL。
 * @post 面掩码、版本字段和比例系数为有效值。
 * @note 测试辅助函数。
 */
static void make_valid_record(app_calibration_record_t *record) {
  App_CalibrationRecord_InitDefaults(record);
  record->face_mask = APP_FACE_ALL;
  record->completed_axis_mask = APP_CALIBRATION_AXIS_COMPLETE_MASK;
  record->gyro_bias_radps[0] = 0.01f;
}

/**
 * @brief 验证默认值、提交成功和失败路径。
 * @post 默认比例系数为 1，成功提交更新状态，Flash 失败锁存。
 * @note 使用记录和滤波器替身。
 */
static void test_init_and_commit(void) {
  filter_imu_calibration_t filter = {0};
  app_calibration_record_store_t store;
  assert(App_CalibrationRecord_Init(&store, &filter));
  assert(store.active_record.accel_scale_ratio[0] == 1.0f);
  assert(s_set_config_calls == 1u);

  app_calibration_record_t record;
  make_valid_record(&record);
  assert(App_CalibrationRecord_Commit(&store, &record));
  assert(s_append_calls == 1u);
  assert(App_CalibrationRecord_HasFlashRecord(&store));
  assert(!App_CalibrationRecord_HasFlashFailure(&store));
  assert(store.active_record.face_mask == APP_FACE_ALL);

  s_append_result = false;
  assert(!App_CalibrationRecord_Commit(&store, &record));
  assert(App_CalibrationRecord_HasFlashFailure(&store));
}

/**
 * @brief 验证有效记录加载。
 * @post Flash 初始化成功时加载记录并应用配置。
 * @note 使用 Flash 替身。
 */
static void test_load(void) {
  filter_imu_calibration_t filter = {0};
  app_calibration_record_store_t store;
  assert(App_CalibrationRecord_Init(&store, &filter));
  make_valid_record(&s_loaded_record);
  assert(App_CalibrationRecord_Load(&store));
  assert(App_CalibrationRecord_HasFlashRecord(&store));
  assert(store.active_record.face_mask == APP_FACE_ALL);

  s_store_init_result = false;
  assert(!App_CalibrationRecord_Load(&store));
  assert(App_CalibrationRecord_HasFlashFailure(&store));
}

/**
 * @brief 运行校准记录契约测试。
 * @return 全部断言通过后返回 0。
 * @post 测试不保留外部资源。
 * @note 任一断言失败会终止进程。
 */
int main(void) {
  test_init_and_commit();
  test_load();
  return 0;
}
