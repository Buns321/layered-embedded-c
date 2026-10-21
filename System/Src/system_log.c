//
// Created by Buns_ on 2026/10/21.
//

#include "system_log.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#if defined(STM32F4xx)
#include "stm32f4xx_hal.h"
#elif defined(STM32F1xx)
#include "stm32f1xx_hal.h"
#else
#error "Unsupported STM32 family"
#endif

#define SYS_LOG_LINE_CAPACITY 192u

static sys_log_base_t *s_log_sink;
static Sys_Log_Level s_log_level = SYS_LOG_LEVEL_INFO;
static volatile bool s_log_locked;
static volatile uint32_t s_drop_bytes;
static volatile uint32_t s_contention_count;

static uint32_t Sys_Log_EnterExclusive(void) {
  uint32_t primask = __get_PRIMASK();
  __disable_irq();
  return primask;
}

static void Sys_Log_ExitExclusive(uint32_t primask) {
  __set_PRIMASK(primask);
}

static const char *Sys_Log_LevelTag(Sys_Log_Level level) {
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

static void Sys_Log_IncrementCounter(volatile uint32_t *counter) {
  uint32_t primask = Sys_Log_EnterExclusive();
  (*counter)++;
  Sys_Log_ExitExclusive(primask);
}

void Sys_Log_Init(sys_log_base_t *sink) {
  s_log_sink = sink;
  s_drop_bytes = 0u;
  s_contention_count = 0u;
  s_log_locked = false;
}

void Sys_Log_SetSink(sys_log_base_t *sink) {
  s_log_sink = sink;
}

void Sys_Log_SetLevel(Sys_Log_Level level) {
  if ((uint32_t)level <= (uint32_t)SYS_LOG_LEVEL_NONE) {
    s_log_level = level;
  } else {
    s_log_level = SYS_LOG_LEVEL_NONE;
  }
}

Sys_Log_Level Sys_Log_GetLevel(void) {
  return s_log_level;
}

bool Sys_Log_IsReady(void) {
  return s_log_sink != NULL;
}

void Sys_Log_Write(
  Sys_Log_Level level,
  const char *tag,
  const char *format,
  ...
) {
  char line[SYS_LOG_LINE_CAPACITY];

  if (
    s_log_sink == NULL ||
    tag == NULL        ||
    format == NULL     ||
    (uint32_t)level > (uint32_t)SYS_LOG_LEVEL_DEBUG ||
    level < s_log_level
  ) {
    return;
  }

  if (!Sys_Log_TryLock()) {
    Sys_Log_IncrementCounter(&s_contention_count);
    return;
  }

  uint32_t now_ms = HAL_GetTick();
  int length = snprintf(
    line,
    sizeof(line),
    "[%lu][%s][%s] ",
    (unsigned long)now_ms,
    Sys_Log_LevelTag(level),
    tag
  );

  if (length < 0 || (uint32_t)length >= sizeof(line)) {
    Sys_Log_IncrementCounter(&s_drop_bytes);
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
    Sys_Log_IncrementCounter(&s_drop_bytes);
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
    s_log_sink,
    (const uint8_t *)line,
    used
  );

  if (accepted < used) {
    s_drop_bytes += used - accepted;
  }

  Sys_Log_Unlock();
}

void Sys_Log_EmergencyRaw(const void *data, uint32_t size) {
  if (
    s_log_sink == NULL ||
    data == NULL       ||
    size == 0u
  ) {
    return;
  }

  (void)Sys_Log_Base_Write(
    s_log_sink,
    (const uint8_t *)data,
    size
  );
}

uint32_t Sys_Log_GetDropBytes(void) {
  return s_drop_bytes;
}

uint32_t Sys_Log_GetContentionCount(void) {
  return s_contention_count;
}
