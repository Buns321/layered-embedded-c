//
// Created by Buns_ on 2026/10/21.
//

/**
 * @file telemetry_base.h
 * @brief Protocol-independent telemetry device interface.
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_TELEMETRY_BASE_H
#define LAYERED_EMBEDDED_C_TELEMETRY_BASE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Maximum number of telemetry channels. */
#ifndef TELEMETRY_CHANNEL_CAPACITY
#define TELEMETRY_CHANNEL_CAPACITY 16u
#endif

_Static_assert(
  TELEMETRY_CHANNEL_CAPACITY > 0u,
  "TELEMETRY_CHANNEL_CAPACITY must be positive"
);
_Static_assert(
  TELEMETRY_CHANNEL_CAPACITY <= 32u,
  "TELEMETRY_CHANNEL_CAPACITY must not exceed 32"
);

#if TELEMETRY_CHANNEL_CAPACITY == 32u
#define TELEMETRY_CHANNEL_MASK UINT32_MAX
#else
#define TELEMETRY_CHANNEL_MASK \
  ((uint32_t)((1u << TELEMETRY_CHANNEL_CAPACITY) - 1u))
#endif

/**
 * @brief One telemetry sample.
 * @note value index and valid_mask bit index correspond directly.
 */
typedef struct {
  float    value[TELEMETRY_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} telemetry_sample_t;

/**
 * @brief Telemetry send status.
 */
typedef enum {
  DEV_TELEMETRY_OK = 0,
  DEV_TELEMETRY_BUSY,
  DEV_TELEMETRY_ERROR,
} Dev_Telemetry_Status;

/**
 * @brief Telemetry backend status snapshot.
 */
typedef struct {
  bool     is_busy;
  uint32_t sent_frames;
  uint32_t dropped_frames;
  uint32_t error_count;
} telemetry_status_t;

typedef struct telemetry_base telemetry_base_t;

/**
 * @brief Concrete telemetry protocol operation table.
 * @note send is non-blocking and must accept a complete frame or no frame.
 */
typedef struct telemetry_ops {
  Dev_Telemetry_Status (*send)(
    telemetry_base_t *this,
    const telemetry_sample_t *sample
  );
  void (*get_status)(
    telemetry_base_t *this,
    telemetry_status_t *status
  );
} telemetry_ops_t;

/**
 * @brief Telemetry device abstract object.
 */
typedef struct telemetry_base {
  const telemetry_ops_t *ops;
  const char            *name;
} telemetry_base_t;

/** @brief Initialize a telemetry device and bind its static operations. */
void Dev_Telemetry_Base_Init(
  telemetry_base_t *this,
  const char *name,
  const telemetry_ops_t *ops
);

/**
 * @brief Submit one protocol frame.
 * @return OK when accepted, BUSY when the transport is occupied, ERROR on
 *         malformed input or encoding failure.
 */
Dev_Telemetry_Status Dev_Telemetry_Base_Send(
  telemetry_base_t *this,
  const telemetry_sample_t *sample
);

/** @brief Copy the current backend status snapshot. */
void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *this,
  telemetry_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_TELEMETRY_BASE_H
