#include "platform_can_backend.h"
#include "platform_can.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static Platform_StatusTypeDef s_transmit_status;
static Platform_StatusTypeDef s_filter_status;
static uint32_t s_backend_error;
static uint32_t s_event_count;
static Platform_CAN_Event s_last_event;

bool platform_can_backend_is_valid(Platform_CAN_Instance can) {
  return can == PLATFORM_CAN_1;
}

Platform_StatusTypeDef platform_can_backend_configure_filter(
  Platform_CAN_Instance can,
  const Platform_CAN_FilterConfig *config
) {
  (void)can;
  (void)config;
  return s_filter_status;
}

Platform_StatusTypeDef platform_can_backend_start(
  Platform_CAN_Instance can
) {
  (void)can;
  return Platform_OK;
}

Platform_StatusTypeDef platform_can_backend_stop(
  Platform_CAN_Instance can
) {
  (void)can;
  return Platform_OK;
}

Platform_StatusTypeDef platform_can_backend_transmit(
  Platform_CAN_Instance can,
  const Platform_CAN_Frame *frame,
  uint32_t timeout
) {
  (void)can;
  (void)frame;
  (void)timeout;
  return s_transmit_status;
}

Platform_StatusTypeDef platform_can_backend_receive(
  Platform_CAN_Instance can,
  Platform_CAN_FIFO fifo,
  Platform_CAN_Frame *frame,
  uint32_t timeout
) {
  (void)can;
  (void)fifo;
  (void)frame;
  (void)timeout;
  return Platform_OK;
}

Platform_StatusTypeDef platform_can_backend_activate_notification(
  Platform_CAN_Instance can,
  uint32_t notifications
) {
  (void)can;
  (void)notifications;
  return Platform_OK;
}

Platform_StatusTypeDef platform_can_backend_deactivate_notification(
  Platform_CAN_Instance can,
  uint32_t notifications
) {
  (void)can;
  (void)notifications;
  return Platform_OK;
}

uint32_t platform_can_backend_get_error(Platform_CAN_Instance can) {
  (void)can;
  return s_backend_error;
}

static void can_callback(
  Platform_CAN_Instance can,
  const Platform_CAN_Event *event,
  void *context
) {
  assert(can == PLATFORM_CAN_1);
  assert(event != NULL);
  assert(context == &s_event_count);
  s_event_count++;
  s_last_event = *event;
}

static void test_can_contract(void) {
  s_transmit_status = Platform_OK;
  s_filter_status = Platform_OK;
  s_backend_error = PLATFORM_CAN_ERROR_NONE;
  s_event_count = 0U;

  Platform_CAN_Frame frame = {
    .id = 0x123U,
    .dlc = 8U,
    .id_type = PLATFORM_CAN_ID_STANDARD,
    .remote = false,
  };
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 10U) == Platform_OK);

  frame.dlc = 9U;
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 10U) ==
         Platform_INVALID_ARGUMENT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_CAN_ERROR_PARAMETER) != 0U);

  frame.dlc = 8U;
  s_transmit_status = Platform_TIMEOUT;
  assert(Platform_CAN_Transmit(PLATFORM_CAN_1, &frame, 1U) ==
         Platform_TIMEOUT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_COMMON_ERROR_TIMEOUT) != 0U);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_CAN_ERROR_TIMEOUT) != 0U);

  const Platform_CAN_FilterConfig filter = {
    .bank = 0U,
    .id = 0x123U,
    .mask = 0x7FFU,
    .mode = PLATFORM_CAN_FILTER_MASK,
    .fifo = PLATFORM_CAN_FIFO_0,
    .id_type = PLATFORM_CAN_ID_STANDARD,
    .remote = false,
    .enabled = true,
  };
  assert(Platform_CAN_ConfigureFilter(PLATFORM_CAN_1, &filter) == Platform_OK);

  s_filter_status = Platform_INVALID_ARGUMENT;
  assert(Platform_CAN_ConfigureFilter(PLATFORM_CAN_1, &filter) ==
         Platform_INVALID_ARGUMENT);
  assert((Platform_CAN_GetLastError(PLATFORM_CAN_1) &
          PLATFORM_COMMON_ERROR_PARAMETER) != 0U);

  const platform_can_callbacks_t callbacks = {
    .callback = can_callback,
    .context = &s_event_count,
  };
  assert(Platform_CAN_RegisterCallbacks(PLATFORM_CAN_1, &callbacks) ==
         Platform_OK);
  assert(Platform_CAN_RegisterCallbacks(PLATFORM_CAN_1, &callbacks) ==
         Platform_BUSY);

  const Platform_CAN_Event event = {
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
