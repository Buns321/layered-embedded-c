/**
 * @file can_contract_test.c
 * @brief CAN Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_can_backend.h"
#include "platform_can.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_transmit_status;
static platform_status_t s_filter_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static platform_can_event_t s_last_event;

bool platform_can_backend_is_valid(platform_can_instance_t can) {
  return can == PLATFORM_CAN_1;
}

platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
) {
  (void)can;
  (void)config;
  return s_filter_status;
}

platform_status_t platform_can_backend_start(
  platform_can_instance_t can
) {
  (void)can;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
) {
  (void)can;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
) {
  (void)can;
  (void)frame;
  (void)timeout;
  return s_transmit_status;
}

platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
) {
  (void)can;
  (void)fifo;
  (void)frame;
  (void)timeout;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  (void)can;
  (void)notifications;
  return PLATFORM_STATUS_OK;
}

platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  (void)can;
  (void)notifications;
  return PLATFORM_STATUS_OK;
}

uint32_t platform_can_backend_get_error(platform_can_instance_t can) {
  (void)can;
  return s_backend_error;
}

static void can_callback(
  platform_can_instance_t can,
  const platform_can_event_t *event,
  void *context
) {
  assert(can == PLATFORM_CAN_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

static void test_can_contract(void) {
  s_transmit_status = PLATFORM_STATUS_OK;
  s_filter_status = PLATFORM_STATUS_OK;
  s_backend_error = PLATFORM_CAN_ERROR_NONE;
  s_event_count = 0U;

  platform_can_frame_t frame = {
    .id = 0x123U,
    .dlc = 8U,
    .id_type = PLATFORM_CAN_ID_STANDARD,
    .remote = false,
  };
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 10U) == PLATFORM_STATUS_OK);

  frame.dlc = 9U;
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 10U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_CAN_ERROR_PARAMETER) != 0U);

  frame.dlc = 8U;
  s_transmit_status = PLATFORM_STATUS_TIMEOUT;
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 1U) ==
         PLATFORM_STATUS_TIMEOUT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_ERROR_TIMEOUT) != 0U);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_CAN_ERROR_TIMEOUT) != 0U);

  const platform_can_filter_config_t filter = {
    .bank = 0U,
    .id = 0x123U,
    .mask = 0x7FFU,
    .mode = PLATFORM_CAN_FILTER_MASK,
    .fifo = PLATFORM_CAN_FIFO_0,
    .id_type = PLATFORM_CAN_ID_STANDARD,
    .remote = false,
    .enabled = true,
  };
  assert(Platform_CAN_ConfigureFilter(PLATFORM_CAN_1, &filter) == PLATFORM_STATUS_OK);

  s_filter_status = PLATFORM_STATUS_INVALID_ARGUMENT;
  assert(Platform_CAN_ConfigureFilter(PLATFORM_CAN_1, &filter) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  const platform_can_callback_binding_t callbacks = {
    .callback = can_callback,
    .context = &s_event_count,
  };
  assert(Platform_CAN_RegisterCallback(PLATFORM_CAN_1, &callbacks) ==
         PLATFORM_STATUS_OK);
  assert(Platform_CAN_RegisterCallback(PLATFORM_CAN_1, &callbacks) ==
         PLATFORM_STATUS_BUSY);

  const platform_can_event_t event = {
    .type = PLATFORM_CAN_EVENT_ERROR,
    .error = PLATFORM_CAN_ERROR_BUS_OFF,
  };
  platform_can_dispatch_event(PLATFORM_CAN_1, &event);
  assert(s_event_count == 1U);
  assert(s_last_event.type == PLATFORM_CAN_EVENT_ERROR);
  assert(s_last_event.error == PLATFORM_CAN_ERROR_BUS_OFF);
}

int main(void) {
  test_can_contract();
  return 0;
}
