//
// 由 Buns_ 创建于 2026/10/20。
//

#include "filter_distributor.h"
#include "system_math.h"
#include <assert.h>

static void Filter_Distributor_Reset(filter_base_t *this) {
  assert(this);
  filter_distributor_t *self = (filter_distributor_t *)this;

  assert(
    self->route_count > 0u                           &&
    self->route_count <= FILTER_DISTRIBUTOR_CAPACITY
  );

  for (uint8_t i = 0; i < self->route_count; i++) {
    assert(self->routes[i].filter);
    Filter_Base_Reset(self->routes[i].filter);
  }
}

static void Filter_Distributor_Update(filter_base_t *this, const filter_sample_t *input, filter_sample_t *output
) {
  assert(this && input && output);
  filter_distributor_t *self = (filter_distributor_t *)this;

  assert(self->route_count > 0u);
  assert(self->route_count <= FILTER_DISTRIBUTOR_CAPACITY);

  for (uint8_t i = 0; i < self->route_count; i++) {
    const filter_distributor_route_t *route = &self->routes[i];

    assert(
      route->filter                                &&
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
      route->filter,
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
  .update = Filter_Distributor_Update,
  .reset  = Filter_Distributor_Reset,
};

void Filter_Distributor_Init(filter_distributor_t *this, const char *name, const filter_distributor_route_t *routes, uint8_t route_count
) {
  assert(
    this                                       &&
    name                                       &&
    routes                                     &&
    route_count > 0u                           &&
    route_count <= FILTER_DISTRIBUTOR_CAPACITY
  );

  for (uint8_t i = 0; i < route_count; i++) {
    const filter_distributor_route_t *route = &routes[i];

    assert(
      route->filter              &&
      route->filter != &this->filter_base &&
      route->filter->ops         &&
      route->filter->ops->update &&
      route->filter->ops->reset
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

      assert(route->filter != previous_route->filter);

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

  Filter_Base_Init(&this->filter_base, name, &filter_ops_distributor);

  for (uint8_t i = 0; i < FILTER_DISTRIBUTOR_CAPACITY; i++) {
    this->routes[i] = (filter_distributor_route_t){0};
  }

  for (uint8_t i = 0; i < route_count; i++) {
    this->routes[i] = routes[i];
  }

  this->route_count = route_count;

  Filter_Base_Reset(&this->filter_base);
}
