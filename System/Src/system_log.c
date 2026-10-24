//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log.c
 * @brief System 日志门面实现。
 * @ingroup system
 */

#include "system_log.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#if defined(STM32F4xx)
#include "stm32f4xx.h"
#elif defined(STM32F1xx)
#include "stm32f1xx.h"
#endif

#ifndef SYS_LOG_GET_TIME_MS
#if defined(SYS_LOG_EXTERNAL_TIME_SOURCE)
extern uint32_t SYS_LOG_EXTERNAL_TIME_SOURCE(void);
#define SYS_LOG_GET_TIME_MS() SYS_LOG_EXTERNAL_TIME_SOURCE()
#elif defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#define SYS_LOG_GET_TIME_MS() HAL_GetTick()
#elif defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#define SYS_LOG_GET_TIME_MS() HAL_GetTick()
#else
#define SYS_LOG_GET_TIME_MS() 0u
#endif
#endif

#define SYS_LOG_LINE_CAPACITY 192u
#define SYS_LOG_FAST_READY_MASK_ (1u << 31)

volatile uint32_t g_sys_log_fast_mask;
static sys_log_base_t *volatile s_log_sink;
static sys_log_level_t volatile s_log_level = SYS_LOG_LEVEL_INFO;
static volatile bool s_log_locked;
static volatile uint32_t s_drop_bytes;
static volatile uint32_t s_contention_count;

static uint32_t Sys_Log_EnterExclusive(void) {
#if defined(STM32F4xx) || defined(STM32F1xx)
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
#else
  return 0u;
#endif
}

static void Sys_Log_ExitExclusive(uint32_t primask) {
#if defined(STM32F4xx) || defined(STM32F1xx)
  __set_PRIMASK(primask);
#else
  (void)primask;
#endif
}

static const char *Sys_Log_GetLevelTag(sys_log_level_t level) {
  switch (level) {
    case SYS_LOG_LEVEL_ERROR: return "E";
    case SYS_LOG_LEVEL_WARN:  return "W";
    case SYS_LOG_LEVEL_INFO:  return "I";
    case SYS_LOG_LEVEL_DEBUG: return "D";
    default:                  return "?";
  }
}

static bool Sys_Log_TryLock(void) {
  bool acquired = false;
  uint32_t primask = Sys_Log_EnterExclusive();

  if (!s_log_locked) {
    s_log_locked = true;
    acquired = true;
  }
  Sys_Log_ExitExclusive(primask);

  return acquired;
}

static void Sys_Log_Unlock(void) {
  uint32_t primask = Sys_Log_EnterExclusive();
  s_log_locked = false;
  Sys_Log_ExitExclusive(primask);
}

static void Sys_Log_AddCounter(
  volatile uint32_t *counter,
  uint32_t amount
) {
  if (amount == 0u) {
    return;
  }

  uint32_t primask = Sys_Log_EnterExclusive();
  *counter += amount;
  Sys_Log_ExitExclusive(primask);
}

static uint32_t Sys_Log_ReadCounter(volatile uint32_t *counter) {
  uint32_t primask = Sys_Log_EnterExclusive();
  uint32_t value = *counter;
  Sys_Log_ExitExclusive(primask);
  return value;
}

static sys_log_base_t *Sys_Log_GetSink(void) {
  uint32_t primask = Sys_Log_EnterExclusive();
  sys_log_base_t *sink_h = s_log_sink;
  Sys_Log_ExitExclusive(primask);
  return sink_h;
}

static void Sys_Log_UpdateFastMaskLocked(void) {
  uint32_t mask = 0u;

  if (s_log_sink != NULL) {
    mask = SYS_LOG_FAST_READY_MASK_;

    if (s_log_level != SYS_LOG_LEVEL_NONE) {
      uint32_t level = (uint32_t)s_log_level;
      mask |= (1u << (level + 1u)) - 1u;
    }
  }

  g_sys_log_fast_mask = mask;
}

void Sys_Log_Init(sys_log_base_t *sink_h) {
  uint32_t primask = Sys_Log_EnterExclusive();
  s_log_sink = sink_h;
  s_drop_bytes = 0u;
  s_contention_count = 0u;
  s_log_locked = false;
  Sys_Log_UpdateFastMaskLocked();
  Sys_Log_ExitExclusive(primask);
}

void Sys_Log_SetSink(sys_log_base_t *sink_h) {
  uint32_t primask = Sys_Log_EnterExclusive();
  s_log_sink = sink_h;
  Sys_Log_UpdateFastMaskLocked();
  Sys_Log_ExitExclusive(primask);
}

void Sys_Log_SetLevel(sys_log_level_t level) {
  uint32_t primask = Sys_Log_EnterExclusive();
  if ((uint32_t)level <= (uint32_t)SYS_LOG_LEVEL_NONE) {
    s_log_level = level;
  } else {
    s_log_level = SYS_LOG_LEVEL_NONE;
  }
  Sys_Log_UpdateFastMaskLocked();
  Sys_Log_ExitExclusive(primask);
}

sys_log_level_t Sys_Log_GetLevel(void) {
  uint32_t primask = Sys_Log_EnterExclusive();
  sys_log_level_t level = s_log_level;
  Sys_Log_ExitExclusive(primask);
  return level;
}

bool Sys_Log_IsReady(void) {
  return (g_sys_log_fast_mask & SYS_LOG_FAST_READY_MASK_) != 0u;
}

void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
) {
  char line[SYS_LOG_LINE_CAPACITY];

  if (
    tag == NULL    ||
    format == NULL ||
    (uint32_t)level > (uint32_t)SYS_LOG_LEVEL_DEBUG ||
    !Sys_Log_IsEnabled(level)
  ) {
    return;
  }

  if (!Sys_Log_TryLock()) {
    Sys_Log_AddCounter(&s_contention_count, 1u);
    return;
  }

  sys_log_base_t *sink_h = Sys_Log_GetSink();
  if (sink_h == NULL) {
    Sys_Log_Unlock();
    return;
  }

  uint32_t now_ms = SYS_LOG_GET_TIME_MS();
  int length = snprintf(
    line,
    sizeof(line),
    "[%lu][%s][%s] ",
    (unsigned long)now_ms,
    Sys_Log_GetLevelTag(level),
    tag
  );

  if (length < 0 || (uint32_t)length >= sizeof(line)) {
    Sys_Log_Unlock();
    return;
  }

  va_list args;
  va_start(args, format);
  int message_length = vsnprintf(
    &line[length],
    sizeof(line) - (uint32_t)length,
    format,
    args
  );
  va_end(args);

  if (message_length < 0) {
    Sys_Log_Unlock();
    return;
  }

  uint32_t used = (uint32_t)length + (uint32_t)message_length;
  if (used > sizeof(line) - 3u) {
    used = sizeof(line) - 3u;
  }

  line[used++] = '\r';
  line[used++] = '\n';

  uint32_t accepted = Sys_Log_Base_Write(
    sink_h,
    (const uint8_t *)line,
    used
  );

  if (accepted < used) {
    Sys_Log_AddCounter(&s_drop_bytes, used - accepted);
  }

  Sys_Log_Unlock();
}

void Sys_Log_EmergencyRaw(const void *data, uint32_t size) {
  sys_log_base_t *sink_h = Sys_Log_GetSink();

  if (
    sink_h == NULL ||
    data == NULL   ||
    size == 0u
  ) {
    return;
  }

  (void)Sys_Log_Base_Write(
    sink_h,
    (const uint8_t *)data,
    size
  );
}

uint32_t Sys_Log_GetDropBytes(void) {
  return Sys_Log_ReadCounter(&s_drop_bytes);
}

uint32_t Sys_Log_GetContentionCount(void) {
  return Sys_Log_ReadCounter(&s_contention_count);
}
