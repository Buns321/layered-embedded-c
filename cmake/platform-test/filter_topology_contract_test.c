/**
 * @file filter_topology_contract_test.c
 * @brief Filter chain 与 distributor 拓扑合同测试。
 * @ingroup tests
 */

#include "filter_chain.h"
#include "filter_distributor.h"
#include "filter_passthrough.h"
#include <assert.h>
#include <stddef.h>

/**
 * @brief 验证 chain 参数错误、拓扑错误和正常运行。
 * @post 非法配置返回 INVALID_CONFIG，合法链保持样本值并完成 reset。
 * @note 只使用 passthrough 子滤波器，不涉及数学变换。
 */
static void test_chain(void) {
  filter_passthrough_t passthrough_a = {0};
  filter_passthrough_t passthrough_b = {0};
  assert(
    Filter_Passthrough_Init(&passthrough_a, "passthrough-a") ==
    FILTER_STATUS_OK
  );
  assert(
    Filter_Passthrough_Init(&passthrough_b, "passthrough-b") ==
    FILTER_STATUS_OK
  );

  filter_base_t *stages[] = {
    &passthrough_a.filter_base,
    &passthrough_b.filter_base,
  };
  filter_chain_t chain = {0};
  chain.stage_count = 77u;

  assert(
    Filter_Chain_Init(NULL, "chain", stages, 2u) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Chain_Init(&chain, NULL, stages, 2u) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Chain_Init(&chain, "chain", NULL, 2u) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Chain_Init(&chain, "chain", stages, 0u) ==
    FILTER_STATUS_INVALID_CONFIG
  );
  assert(chain.stage_count == 77u);

  filter_base_t *null_stages[] = {
    NULL,
  };
  assert(
    Filter_Chain_Init(&chain, "chain", null_stages, 1u) ==
    FILTER_STATUS_INVALID_CONFIG
  );

  filter_base_t *self_stages[] = {
    &chain.filter_base,
  };
  assert(
    Filter_Chain_Init(&chain, "chain", self_stages, 1u) ==
    FILTER_STATUS_INVALID_CONFIG
  );

  filter_base_t *duplicate_stages[] = {
    &passthrough_a.filter_base,
    &passthrough_a.filter_base,
  };
  assert(
    Filter_Chain_Init(&chain, "chain", duplicate_stages, 2u) ==
    FILTER_STATUS_INVALID_CONFIG
  );
  assert(chain.stage_count == 77u);

  assert(
    Filter_Chain_Init(&chain, "chain", stages, 2u) ==
    FILTER_STATUS_OK
  );

  filter_sample_t input = {
    .valid_mask = (1u << 2),
    .time_ms = 123u,
  };
  input.value[2] = 9.0f;
  filter_sample_t output = {0};
  Filter_Base_Update(&chain.filter_base, &input, &output);
  assert(output.valid_mask == input.valid_mask);
  assert(output.value[2] == 9.0f);
  assert(output.time_ms == input.time_ms);
}

/**
 * @brief 验证 distributor 冲突拒绝和通道映射。
 * @post 重复滤波器或重复输出被拒绝；合法路由把局部输出写回全局通道。
 * @note 子滤波器为 passthrough，仍经过完整 base 清洗路径。
 */
static void test_distributor(void) {
  filter_passthrough_t passthrough_a = {0};
  filter_passthrough_t passthrough_b = {0};
  assert(
    Filter_Passthrough_Init(&passthrough_a, "route-a") ==
    FILTER_STATUS_OK
  );
  assert(
    Filter_Passthrough_Init(&passthrough_b, "route-b") ==
    FILTER_STATUS_OK
  );

  filter_distributor_route_t routes[2] = {
    {
      .filter_h = &passthrough_a.filter_base,
      .input_index = {0u},
      .output_index = {0u},
      .input_count = 1u,
      .output_count = 1u,
    },
    {
      .filter_h = &passthrough_b.filter_base,
      .input_index = {1u},
      .output_index = {1u},
      .input_count = 1u,
      .output_count = 1u,
    },
  };

  filter_distributor_t distributor = {0};
  distributor.route_count = 77u;
  assert(
    Filter_Distributor_Init(NULL, "distributor", routes, 2u) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Distributor_Init(&distributor, "distributor", NULL, 2u) ==
    FILTER_STATUS_INVALID_ARGUMENT
  );
  assert(
    Filter_Distributor_Init(&distributor, "distributor", routes, 0u) ==
    FILTER_STATUS_INVALID_CONFIG
  );
  assert(distributor.route_count == 77u);

  filter_distributor_route_t duplicate_input_route = routes[0];
  duplicate_input_route.input_count = 2u;
  duplicate_input_route.input_index[1] = duplicate_input_route.input_index[0];
  assert(
    Filter_Distributor_Init(
      &distributor,
      "distributor",
      &duplicate_input_route,
      1u
    ) == FILTER_STATUS_INVALID_CONFIG
  );

  filter_distributor_route_t duplicate_filter_routes[2] = {
    routes[0],
    routes[0],
  };
  duplicate_filter_routes[1].input_index[0] = 1u;
  duplicate_filter_routes[1].output_index[0] = 1u;
  assert(
    Filter_Distributor_Init(
      &distributor,
      "distributor",
      duplicate_filter_routes,
      2u
    ) == FILTER_STATUS_INVALID_CONFIG
  );

  filter_distributor_route_t duplicate_output_routes[2] = {
    routes[0],
    routes[1],
  };
  duplicate_output_routes[1].output_index[0] = 0u;
  assert(
    Filter_Distributor_Init(
      &distributor,
      "distributor",
      duplicate_output_routes,
      2u
    ) == FILTER_STATUS_INVALID_CONFIG
  );
  assert(distributor.route_count == 77u);

  assert(
    Filter_Distributor_Init(
      &distributor,
      "distributor",
      routes,
      2u
    ) == FILTER_STATUS_OK
  );

  filter_sample_t input = {
    .valid_mask = (1u << 0) | (1u << 1),
    .time_ms = 321u,
  };
  input.value[0] = 1.5f;
  input.value[1] = 2.5f;
  filter_sample_t output = {0};
  Filter_Base_Update(&distributor.filter_base, &input, &output);
  assert(output.valid_mask == input.valid_mask);
  assert(output.value[0] == 1.5f);
  assert(output.value[1] == 2.5f);
  assert(output.time_ms == input.time_ms);
}

/**
 * @brief 运行 Filter 拓扑合同测试。
 * @return 固定返回 0；全部断言通过才会到达返回语句。
 * @post 测试栈对象失效，不保留全局状态。
 * @note 仅供 CTest 启动。
 */
int main(void) {
  test_chain();
  test_distributor();
  return 0;
}
