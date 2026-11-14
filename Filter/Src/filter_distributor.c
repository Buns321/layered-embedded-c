//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file filter_distributor.c
 * @brief 多路滤波器分发器实现。
 * @ingroup filter
 */

#include "filter_distributor.h"
#include <stddef.h>

typedef struct filter_distributor_init_data_s {
  const filter_distributor_route_t *routes;
  uint8_t route_count;
} filter_distributor_init_data_t;

/**
 * @brief 校验并初始化 distributor 具体配置。
 * @param filter_h 待构造的 distributor 对象；用于拒绝自引用，
 *        首次写入前完成全部校验。
 * @param filter_config 非空基配置；data 指向 filter_distributor_init_data_t。
 * @retval FILTER_STATUS_OK 路由表和全部路由有效。
 * @retval FILTER_STATUS_INVALID_ARGUMENT 路由表为空。
 * @retval FILTER_STATUS_INVALID_CONFIG 路由数量、滤波器、通道索引或输出冲突非法。
 * @pre filter_config 指向完整的 distributor 私有配置。
 * @post 成功时全部路由槽位和路由数已写入；失败时不修改分发器对象。
 * @note 私有构造回调，不分配动态内存。
 */
static filter_status_t filter_distributor_init(
  filter_base_t *filter_h,
  const filter_config_t *filter_config
) {
  filter_distributor_t *filter_distributor_h =
    (filter_distributor_t *)filter_h;
  const filter_distributor_init_data_t *init_data =
    (const filter_distributor_init_data_t *)filter_config->data;

  if (init_data == NULL || init_data->routes == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }
  if (
    init_data->route_count == 0u ||
    init_data->route_count > FILTER_DISTRIBUTOR_CAPACITY
  ) {
    return FILTER_STATUS_INVALID_CONFIG;
  }

  for (uint8_t i = 0; i < init_data->route_count; i++) {
    const filter_distributor_route_t *route = &init_data->routes[i];

    if (
      route->filter_h == NULL ||
      route->filter_h == filter_h ||
      route->filter_h->ops == NULL ||
      route->input_count == 0u ||
      route->input_count > FILTER_ROUTE_CAPACITY ||
      route->output_count == 0u ||
      route->output_count > FILTER_ROUTE_CAPACITY
    ) {
      return FILTER_STATUS_INVALID_CONFIG;
    }

    for (uint8_t j = 0; j < route->input_count; j++) {
      if (route->input_index[j] >= FILTER_CHANNEL_CAPACITY) {
        return FILTER_STATUS_INVALID_CONFIG;
      }

      for (uint8_t k = 0; k < j; k++) {
        if (route->input_index[j] == route->input_index[k]) {
          return FILTER_STATUS_INVALID_CONFIG;
        }
      }
    }

    for (uint8_t j = 0; j < route->output_count; j++) {
      if (route->output_index[j] >= FILTER_CHANNEL_CAPACITY) {
        return FILTER_STATUS_INVALID_CONFIG;
      }

      for (uint8_t k = 0; k < j; k++) {
        if (route->output_index[j] == route->output_index[k]) {
          return FILTER_STATUS_INVALID_CONFIG;
        }
      }
    }

    for (uint8_t previous = 0; previous < i; previous++) {
      const filter_distributor_route_t *previous_route =
        &init_data->routes[previous];

      if (route->filter_h == previous_route->filter_h) {
        return FILTER_STATUS_INVALID_CONFIG;
      }

      for (uint8_t j = 0; j < route->output_count; j++) {
        for (uint8_t k = 0; k < previous_route->output_count; k++) {
          if (
            route->output_index[j] ==
            previous_route->output_index[k]
          ) {
            return FILTER_STATUS_INVALID_CONFIG;
          }
        }
      }
    }
  }

  for (uint8_t i = 0; i < FILTER_DISTRIBUTOR_CAPACITY; i++) {
    filter_distributor_h->routes[i] =
      i < init_data->route_count ? init_data->routes[i] :
      (filter_distributor_route_t){0};
  }
  filter_distributor_h->route_count = init_data->route_count;
  return FILTER_STATUS_OK;
}

/**
 * @brief 重置全部分发通道。
 * @param filter_h 已初始化的分发器基对象，不能为 NULL。
 * @pre route_count 在 1..FILTER_DISTRIBUTOR_CAPACITY 内，全部 route filter 指针有效。
 * @post 每条路由的滤波器均执行 reset；路由表、通道映射和分发器名称不变。
 * @note 路由滤波器在初始化时已保证唯一；不分配动态内存，非线程安全。
 */
static void filter_distributor_reset(filter_base_t *filter_h) {
  filter_distributor_t *filter_distributor_h = (filter_distributor_t *)filter_h;

  for (uint8_t i = 0; i < filter_distributor_h->route_count; i++) {
    Filter_Base_Reset(filter_distributor_h->routes[i].filter_h);
  }
}

/**
 * @brief 按通道分发输入并汇总输出。
 * @param filter_h 已初始化的分发器基对象，不能为 NULL。
 * @param sample base 已预复制并清洗的全局样本；各路由按 input_index 选取通道。
 * @pre 路由配置已在初始化阶段校验。
 * @post 每条有有效输入的路由执行一次滤波；仅把 local_output 中有效且有限的通道按 output_index 写回。
 *       无有效输入的路由跳过，未写回的全局通道保持 base 预复制值和有效位。
 * @note 子滤波器输出由 Filter_Base_Update 统一清洗；不分配动态内存，单写者调用。
 */
static void filter_distributor_update(
  filter_base_t *filter_h,
  filter_sample_t *sample
) {
  filter_distributor_t *filter_distributor_h = (filter_distributor_t *)filter_h;

  for (uint8_t i = 0; i < filter_distributor_h->route_count; i++) {
    const filter_distributor_route_t *route = &filter_distributor_h->routes[i];

    filter_sample_t local_input = {0};
    local_input.time_ms = sample->time_ms;

    for (uint8_t j = 0; j < route->input_count; j++) {
      uint8_t global_index = route->input_index[j];
      uint32_t global_mask = Filter_Sample_GetChannelMask(global_index);

      if ((sample->valid_mask & global_mask) != 0u) {
        local_input.value[j] = sample->value[global_index];
        local_input.valid_mask |= Filter_Sample_GetChannelMask(j);
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
      uint32_t local_mask = Filter_Sample_GetChannelMask(j);
      if ((local_output.valid_mask & local_mask) == 0u) {
        continue;
      }

      float output_value = local_output.value[j];
      uint8_t global_index = route->output_index[j];

      (void)Filter_Sample_WriteFiniteChannel(
        sample,
        global_index,
        output_value
      );
    }
  }
}

static const filter_ops_t filter_ops_distributor = {
  .init = filter_distributor_init,
  .update = filter_distributor_update,
  .reset = filter_distributor_reset,
};

/**
 * @brief 初始化分发器。
 * @param filter_h 待初始化的分发器存储；可处于未初始化状态，不能为 NULL。
 * @param name 非空只读名称；生命周期必须覆盖分发器对象。
 * @param routes 非空路由表；前 route_count 项在调用期间只读。
 * @param route_count 路由数，范围 1..FILTER_DISTRIBUTOR_CAPACITY。
 * @retval FILTER_STATUS_OK 路由表有效，分发器已完成初始化。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h、name 或 routes 为空。
 * @retval FILTER_STATUS_INVALID_CONFIG 路由数量、滤波器、通道索引或输出冲突非法。
 * @pre 每条路由的输入/输出数量在 1..FILTER_ROUTE_CAPACITY 内，索引小于 FILTER_CHANNEL_CAPACITY。
 * @post 路由表复制入对象、剩余槽位清零，并 reset 全部唯一子滤波器。
 * @post 失败时对象保持不变。
 * @note 重复滤波器、同一路由内重复索引和跨路由重复输出均被拒绝。
 */
filter_status_t Filter_Distributor_Init(
  filter_distributor_t *filter_h,
  const char *name,
  const filter_distributor_route_t *routes,
  uint8_t route_count
) {
  if (filter_h == NULL) {
    return FILTER_STATUS_INVALID_ARGUMENT;
  }

  const filter_distributor_init_data_t init_data = {
    .routes = routes,
    .route_count = route_count,
  };
  const filter_config_t config = {
    .name = name,
    .data = &init_data,
  };
  return Filter_Base_Construct(
    &filter_h->filter_base,
    &config,
    &filter_ops_distributor
  );
}
