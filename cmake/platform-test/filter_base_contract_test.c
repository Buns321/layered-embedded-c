/**
 * @file filter_base_contract_test.c
 * @brief Filter 基类公共样本合同测试。
 * @ingroup tests
 */

#include "filter_base.h"
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static uint32_t s_seen_mask;
static uint32_t s_reset_count;
static uint32_t s_init_count;

/**
 * @brief 记录进入算法前的清洗结果并制造非有限输出。
 * @param filter_h 滤波器基对象；本测试替身不访问该参数。
 * @param sample base 已预复制并清洗的可读写样本。
 * @pre sample 非空，且 base 已清除非法掩码位和非有限输入通道。
 * @post 记录看到的掩码，写入一个有限通道、一个非有限通道和非法掩码位。
 * @note 用于验证 base 的输入与输出清洗顺序。
 */
static void filter_base_testupdate(
  filter_base_t *filter_h,
  filter_sample_t *sample
) {
  (void)filter_h;
  s_seen_mask = sample->valid_mask;

  sample->value[0] = 5.0f;
  Filter_Sample_WriteFiniteChannel(sample, 5u, 7.0f);
  Filter_Sample_WriteFiniteChannel(sample, 4u, NAN);
  sample->time_ms = 999u;
}

/**
 * @brief 记录基类 reset 转发。
 * @param filter_h 滤波器基对象；本测试替身不访问该参数。
 * @pre filter_h 非空。
 * @post s_reset_count 加一。
 * @note 不修改滤波器状态。
 */
static void filter_base_testreset(filter_base_t *filter_h) {
  (void)filter_h;
  s_reset_count++;
}

static const filter_ops_t s_filter_base_test_ops = {
  .update = filter_base_testupdate,
  .reset = filter_base_testreset,
};

/**
 * @brief 校验并初始化 Filter 构造器测试配置。
 * @param filter_h 待构造的基对象；本测试替身不读取该参数。
 * @param filter_config 非空基配置；data 指向 bool 有效性标记。
 * @retval FILTER_STATUS_OK 测试配置有效。
 * @retval FILTER_STATUS_INVALID_CONFIG is_valid 为 false。
 * @pre filter_config->data 指向调用期间有效的 bool。
 * @post 成功时 s_init_count 加一；失败时不修改对象。
 * @note 用于验证构造器先执行可失败初始化，再绑定 base。
 */
static filter_status_t filter_base_testinit(
  filter_base_t *filter_h,
  const filter_config_t *filter_config
) {
  (void)filter_h;
  const bool *is_valid = (const bool *)filter_config->data;
  if (is_valid == NULL || !*is_valid) {
    return FILTER_STATUS_INVALID_CONFIG;
  }
  s_init_count++;
  return FILTER_STATUS_OK;
}

static const filter_ops_t s_filter_base_constructor_ops = {
  .init = filter_base_testinit,
  .update = filter_base_testupdate,
  .reset = filter_base_testreset,
};

/**
 * @brief 验证基类初始化的参数错误状态。
 * @post 空对象、空配置、空名称和空 ops 均返回 INVALID_ARGUMENT。
 * @note 只操作栈对象，不访问硬件。
 */
static void test_initialization_errors(void) {
  filter_base_t filter = {0};
  const filter_config_t config = {
    .name = "filter",
  };

  assert(
    Filter_Base_Construct(
      NULL,
      &config,
      &s_filter_base_test_ops
    ) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Base_Construct(
      &filter,
      NULL,
      &s_filter_base_test_ops
    ) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Base_Construct(
      &filter,
      &config,
      NULL
    ) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  const filter_config_t missing_name = {0};
  assert(
    Filter_Base_Construct(
      &filter,
      &missing_name,
      &s_filter_base_test_ops
    ) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
}

/**
 * @brief 验证配置校验失败时对象保持原样。
 * @post validate 返回 INVALID_CONFIG，configure/reset 和对象字段均不执行。
 * @note 该测试明确构造器“校验前不移交对象”的失败语义。
 */
static void test_validation_failure(void) {
  filter_base_t filter = {
    .ops = &s_filter_base_test_ops,
    .name = "sentinel",
  };
  const bool is_valid = false;
  const filter_config_t config = {
    .name = "invalid",
    .data = &is_valid,
  };
  uint32_t init_before = s_init_count;
  uint32_t reset_before = s_reset_count;

  assert(
    Filter_Base_Construct(
      &filter,
      &config,
      &s_filter_base_constructor_ops
    ) == FILTER_STATUS_INVALID_CONFIG
  );
  assert(filter.ops == &s_filter_base_test_ops);
  assert(strcmp(filter.name, "sentinel") == 0);
  assert(s_init_count == init_before);
  assert(s_reset_count == reset_before);
}

/**
 * @brief 验证输入复制、输入清洗、输出清洗和时间戳保护。
 * @post 算法只能看到有限有效通道；最终掩码只含有限输出通道，时间戳恢复为输入值。
 * @note 测试使用 NaN 和正无穷，不依赖具体硬件浮点环境。
 */
static void test_update_contract(void) {
  filter_base_t filter = {0};
  const bool is_valid = true;
  const filter_config_t config = {
    .name = "filter",
    .data = &is_valid,
  };
  s_reset_count = 0u;
  s_init_count = 0u;
  assert(
    Filter_Base_Construct(
      &filter,
      &config,
      &s_filter_base_constructor_ops
    ) == FILTER_STATUS_OK
  );
  assert(s_init_count == 1u);
  assert(s_reset_count == 1u);

  filter_sample_t input = {
    .valid_mask = (1u << 0) | (1u << 1) | (1u << 2) | (1u << 3) | (1u << 16),
    .time_ms = 42u,
  };
  input.value[0] = 1.0f;
  input.value[1] = NAN;
  input.value[2] = INFINITY;
  input.value[3] = 4.0f;

  filter_sample_t output = {0};
  Filter_Base_Update(&filter, &input, &output);

  assert(s_seen_mask == ((1u << 0) | (1u << 3)));
  assert(output.time_ms == input.time_ms);
  assert(output.value[0] == 5.0f);
  assert(output.value[5] == 7.0f);
  assert(output.valid_mask == ((1u << 0) | (1u << 3) | (1u << 5)));

  Filter_Base_Reset(&filter);
  assert(s_reset_count == 2u);
}

/**
 * @brief 验证空回调操作表产生合法透传滤波器。
 * @post 构造成功，update 只保留 base 预复制/清洗结果，reset 为空操作。
 * @note 空回调语义用于减少无状态滤波器样板代码。
 */
static void test_optional_callbacks(void) {
  static const filter_ops_t empty_ops = {0};
  const filter_config_t config = {
    .name = "empty",
  };
  filter_base_t filter = {0};

  assert(
    Filter_Base_Construct(
      &filter,
      &config,
      &empty_ops
    ) == FILTER_STATUS_OK
  );

  filter_sample_t input = {
    .valid_mask = (1u << 0),
    .time_ms = 7u,
  };
  input.value[0] = 3.0f;
  filter_sample_t output = {0};
  Filter_Base_Update(&filter, &input, &output);
  assert(output.valid_mask == input.valid_mask);
  assert(output.value[0] == 3.0f);
  assert(output.time_ms == input.time_ms);
  Filter_Base_Reset(&filter);
}

/**
 * @brief 运行 Filter 基类合同测试。
 * @return 固定返回 0；全部断言通过才会到达返回语句。
 * @post 测试栈对象失效，全局计数保留为本进程测试结果。
 * @note 仅供 CTest 启动。
 */
int main(void) {
  test_initialization_errors();
  test_validation_failure();
  test_update_contract();
  test_optional_callbacks();
  return 0;
}
