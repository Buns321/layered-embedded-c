//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file telemetry_base.h
 * @brief 与具体协议无关的遥测设备接口。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_TELEMETRY_BASE_H
#define LAYERED_EMBEDDED_C_TELEMETRY_BASE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 遥测通道最大数量。 */
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
 * @brief 一次遥测样本。
 * @note value 下标与 valid_mask 位下标一一对应。
 */
typedef struct {
  float    value[TELEMETRY_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} telemetry_sample_t;

/**
 * @brief 遥测发送状态。
 */
typedef enum {
  DEV_TELEMETRY_OK = 0,
  DEV_TELEMETRY_BUSY,
  DEV_TELEMETRY_ERROR,
} Dev_Telemetry_Status;

/**
 * @brief 遥测后端状态快照。
 */
typedef struct {
  bool     is_busy;
  uint32_t sent_frames;
  uint32_t dropped_frames;
  uint32_t error_count;
} telemetry_status_t;

typedef struct telemetry_base telemetry_base_t;

/**
 * @brief 具体遥测协议的操作表。
 * @note send 必须非阻塞，并且只能完整接受一帧或完全不接受。
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
 * @brief 遥测设备抽象对象。
 */
typedef struct telemetry_base {
  const telemetry_ops_t *ops;
  const char            *name;
} telemetry_base_t;

/** @brief 初始化遥测设备并绑定静态操作表。 */
void Dev_Telemetry_Base_Init(
  telemetry_base_t *this,
  const char *name,
  const telemetry_ops_t *ops
);

/**
 * @brief 提交一帧协议数据。
 * @return 接受时返回 OK；传输通道被占用时返回 BUSY；输入格式非法或编码
 *         失败时返回 ERROR。
 */
Dev_Telemetry_Status Dev_Telemetry_Base_Send(
  telemetry_base_t *this,
  const telemetry_sample_t *sample
);

/** @brief 复制当前后端状态快照。 */
void Dev_Telemetry_Base_GetStatus(
  telemetry_base_t *this,
  telemetry_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_TELEMETRY_BASE_H
