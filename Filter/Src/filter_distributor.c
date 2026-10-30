//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_distributor.c
 * @brief 多路滤波器分发器实现。
 * @ingroup filter
 */

#include "filter_distributor.h"
#include "system_math.h"
#include <assert.h>

/**
 * @brief 重置全部分发通道。
 * @param filter_h 已初始化的分发器基对象，不能为 NULL。
 * @pre route_count 在 1..FILTER_DISTRIBUTOR_CAPACITY 内，全部 route filter 指针有效。
 * @post 每条路由的滤波器均执行 reset；路由表、通道映射和分发器名称不变。
 * @note 路由滤波器在初始化时已保证唯一；不分配动态内存，非线程安全。
 */
static void filter_distributor_reset(filter_base_t *filter_h) {
  assert(filter_h);
  filter_distributor_t *filter_distributor_h = (filter_distributor_t *)filter_h;

  assert(
    filter_distributor_h->route_count > 0u                           &&
    filter_distributor_h->route_count <= FILTER_DISTRIBUTOR_CAPACITY
  );

  for (uint8_t i = 0; i < filter_distributor_h->route_count; i++) {
    assert(filter_distributor_h->routes[i].filter_h);
    Filter_Base_Reset(filter_distributor_h->routes[i].filter_h);
  }
}

/**
 * @brief 按通道分发输入并汇总输出。
 * @param filter_h 已初始化的分发器基对象，不能为 NULL。
 * @param input 只读全局输入样本；各路由按 input_index 选取通道。
 * @param output 调用方拥有的全局输出；base 已预复制 input。
 * @pre 路由配置有效，input/output 期间可访问，output 的 time_ms 保持输入时间戳。
 * @post 每条有有效输入的路由执行一次滤波；仅把 local_output 中有效且有限的通道按 output_index 写回。
 *       无有效输入的路由跳过，未写回的全局通道保持 base 预复制值和有效位。
 * @note 非有限滤波结果会触发 assert 并被跳过；不分配动态内存，单写者调用。
 */
static void filter_distributor_update(filter_base_t *filter_h, const filter_sample_t *input, filter_sample_t *output
) {
  assert(filter_h && input && output);
  filter_distributor_t *filter_distributor_h = (filter_distributor_t *)filter_h;

  assert(filter_distributor_h->route_count > 0u);
  assert(filter_distributor_h->route_count <= FILTER_DISTRIBUTOR_CAPACITY);

  for (uint8_t i = 0; i < filter_distributor_h->route_count; i++) {
    const filter_distributor_route_t *route = &filter_distributor_h->routes[i];

    assert(
      route->filter_h                                &&
      route->input_count  >  0u                    &&
      route->input_count  <= FILTER_ROUTE_CAPACITY &&
      route->output_count >  0u                    &&
      route->output_count <= FILTER_ROUTE_CAPACITY
    );

    filter_sample_t local_input = {0};
    local_input.time_ms = input->time_ms;

    for (uint8_t j = 0; j < route->input_count; j++) {
      uint8_t global_index = route->input_index[j];
      uint32_t global_mask = (uint32_t)1u << global_index;

      if ((input->valid_mask & global_mask) != 0u) {
        local_input.value[j] = input->value[global_index];
        local_input.valid_mask |= (uint32_t)1u << j;
      }
    }

    if (local_input.valid_mask == 0u) {
      continue;
    }

    filter_sample_t local_output;
    Filter_Base_Update(
      route->filter_h,
      &local_input,
      &local_output
    );

    for (uint8_t j = 0; j < route->output_count; j++) {
      uint32_t local_mask = (uint32_t)1u << j;
      if ((local_output.valid_mask & local_mask) == 0u) {
        continue;
      }

      float output_value = local_output.value[j];
      assert(SYS_ISFINITE(output_value));
      if (!SYS_ISFINITE(output_value)) {
        continue;
      }

      uint8_t global_index = route->output_index[j];
      uint32_t global_mask = (uint32_t)1u << global_index;

      output->value[global_index] = output_value;
      output->valid_mask |= global_mask;
    }
  }
}

static const filter_ops_t filter_ops_distributor = {
  .update = filter_distributor_update,
  .reset  = filter_distributor_reset,
};

/**
 * @brief 初始化分发器。
 * @param filter_h 待初始化的分发器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖分发器对象。
 * @param routes 非空路由表；前 route_count 项在调用期间只读。
 * @param route_count 路由数，范围 1..FILTER_DISTRIBUTOR_CAPACITY。
 * @pre 每条路由的输入/输出数量在 1..FILTER_ROUTE_CAPACITY 内，索引小于 FILTER_CHANNEL_CAPACITY。
 * @post 路由表复制入对象、剩余槽位清零，并 reset 全部唯一子滤波器。
 * @note Debug 构建拒绝重复滤波器、同一路由内重复索引和跨路由重复输出；仅初始化阶段调用。
 */
void Filter_Distributor_Init(filter_distributor_t *filter_h, const char *name, const filter_distributor_route_t *routes, uint8_t route_count
) {
  assert(
    filter_h                                       &&
    name                                       &&
    routes                                     &&
    route_count > 0u                           &&
    route_count <= FILTER_DISTRIBUTOR_CAPACITY
  );

  for (uint8_t i = 0; i < route_count; i++) {
    const filter_distributor_route_t *route = &routes[i];

    assert(
      route->filter_h              &&
      route->filter_h != &filter_h->filter_base &&
      route->filter_h->ops         &&
      route->filter_h->ops->update &&
      route->filter_h->ops->reset
    );
    assert(route->input_count > 0u);
    assert(route->input_count <= FILTER_ROUTE_CAPACITY);
    assert(route->output_count > 0u);
    assert(route->output_count <= FILTER_ROUTE_CAPACITY);

    for (uint8_t j = 0; j < route->input_count; j++) {
      assert(route->input_index[j] < FILTER_CHANNEL_CAPACITY);

      for (uint8_t k = 0; k < j; k++) {
        assert(route->input_index[j] != route->input_index[k]);
      }
    }

    for (uint8_t j = 0; j < route->output_count; j++) {
      assert(route->output_index[j] < FILTER_CHANNEL_CAPACITY);

      for (uint8_t k = 0; k < j; k++) {
        assert(route->output_index[j] != route->output_index[k]);
      }
    }

    for (uint8_t previous = 0; previous < i; previous++) {
      const filter_distributor_route_t *previous_route = &routes[previous];

      assert(route->filter_h != previous_route->filter_h);

      for (uint8_t j = 0; j < route->output_count; j++) {
        for (
          uint8_t k = 0;
          k < previous_route->output_count;
          k++
        ) {
          assert(
            route->output_index[j] !=
            previous_route->output_index[k]
          );
        }
      }
    }
  }

  Filter_Base_Init(&filter_h->filter_base, name, &filter_ops_distributor);

  for (uint8_t i = 0; i < FILTER_DISTRIBUTOR_CAPACITY; i++) {
    filter_h->routes[i] = (filter_distributor_route_t){0};
  }

  for (uint8_t i = 0; i < route_count; i++) {
    filter_h->routes[i] = routes[i];
  }

  filter_h->route_count = route_count;

  Filter_Base_Reset(&filter_h->filter_base);
}
