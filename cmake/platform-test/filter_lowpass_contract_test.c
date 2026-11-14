/**
 * @file filter_lowpass_contract_test.c
 * @brief 多通道一阶低通滤波器合同测试。
 * @ingroup tests
 */

#include "filter_lowpass.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>

/**
 * @brief 验证初始化、通道配置和状态复位。
 * @post 非法参数返回对应 Filter 状态，合法参数完成配置。
 * @note 使用栈对象，不访问硬件。
 */
static void test_configuration(void) {
  filter_lowpass_t lowpass = {0};

  assert(
    Filter_Lowpass_Init(NULL, "lowpass", 0.01f) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Lowpass_Init(&lowpass, NULL, 0.01f) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Lowpass_Init(&lowpass, "lowpass", 0.0f) ==
    FILTER_STATUS_INVALID_CONFIG
  );
  assert(
    Filter_Lowpass_Init(&lowpass, "lowpass", NAN) ==
    FILTER_STATUS_INVALID_CONFIG
  );
  assert(
    Filter_Lowpass_Init(&lowpass, "lowpass", 0.01f) ==
    FILTER_STATUS_OK
  );
  assert(
    Filter_Lowpass_SetChannelTimeConstant(&lowpass, 0u, 0.02f) ==
    FILTER_STATUS_OK
  );
  assert(
    Filter_Lowpass_SetChannelTimeConstant(
      &lowpass,
      FILTER_CHANNEL_CAPACITY,
      0.02f
    ) == FILTER_STATUS_INVALID_CONFIG
  );
  assert(
    Filter_Lowpass_SetChannelTimeConstant(&lowpass, 0u, -1.0f) ==
    FILTER_STATUS_INVALID_CONFIG
  );
}

/**
 * @brief 验证首样本、时间差、零时间差和 reset 后重新初始化。
 * @post 输出依次为 10、15、15；reset 后首样本直接采用新输入 40。
 * @note 只使用单通道，避免其他通道的隐含状态影响断言。
 */
static void test_filtering(void) {
  filter_lowpass_t lowpass = {0};
  assert(
    Filter_Lowpass_Init(&lowpass, "lowpass", 0.01f) ==
    FILTER_STATUS_OK
  );

  filter_sample_t input = {
    .valid_mask = (1u << 0),
    .time_ms = 100u,
  };
  input.value[0] = 10.0f;
  filter_sample_t output = {0};
  Filter_Base_Update(&lowpass.filter_base, &input, &output);
  assert(output.valid_mask == (1u << 0));
  assert(output.value[0] == 10.0f);

  input.time_ms = 110u;
  input.value[0] = 20.0f;
  Filter_Base_Update(&lowpass.filter_base, &input, &output);
  assert(output.value[0] == 15.0f);

  input.value[0] = 30.0f;
  Filter_Base_Update(&lowpass.filter_base, &input, &output);
  assert(output.value[0] == 15.0f);

  Filter_Base_Reset(&lowpass.filter_base);
  input.time_ms = 120u;
  input.value[0] = 40.0f;
  Filter_Base_Update(&lowpass.filter_base, &input, &output);
  assert(output.value[0] == 40.0f);
}

/**
 * @brief 验证非有限输入不会进入低通状态。
 * @post NaN 输入通道被 base 清除；后续有限样本按首样本直接输出。
 * @note 验证公共清洗与低通状态机的组合行为。
 */
static void test_nonfinite_input(void) {
  filter_lowpass_t lowpass = {0};
  assert(
    Filter_Lowpass_Init(&lowpass, "lowpass", 0.01f) ==
    FILTER_STATUS_OK
  );

  filter_sample_t input = {
    .valid_mask = (1u << 0),
    .time_ms = 10u,
  };
  input.value[0] = NAN;
  filter_sample_t output = {0};
  Filter_Base_Update(&lowpass.filter_base, &input, &output);
  assert(output.valid_mask == 0u);

  input.time_ms = 20u;
  input.value[0] = 3.0f;
  Filter_Base_Update(&lowpass.filter_base, &input, &output);
  assert(output.valid_mask == (1u << 0));
  assert(output.value[0] == 3.0f);
}

/**
 * @brief 运行低通滤波器合同测试。
 * @return 固定返回 0；全部断言通过才会到达返回语句。
 * @post 测试栈对象失效，不保留全局状态。
 * @note 仅供 CTest 启动。
 */
int main(void) {
  test_configuration();
  test_filtering();
  test_nonfinite_input();
  return 0;
}
