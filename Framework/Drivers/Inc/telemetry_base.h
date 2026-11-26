/**
 * @file telemetry_base.h
 * @brief 协议无关遥测设备多态基类（非阻塞整帧接收/丢弃语义）。
 * @addtogroup drivers
 * @{
 */

#ifndef LAYERED_CAR_TELEMETRY_BASE_H
#define LAYERED_CAR_TELEMETRY_BASE_H

#include <stdbool.h>
#include <stdint.h>

#ifndef TELEMETRY_CHANNEL_CAPACITY
#define TELEMETRY_CHANNEL_CAPACITY 16U
#endif

_Static_assert(TELEMETRY_CHANNEL_CAPACITY > 0U, "TELEMETRY_CHANNEL_CAPACITY must be positive");
_Static_assert(TELEMETRY_CHANNEL_CAPACITY <= 32U, "TELEMETRY_CHANNEL_CAPACITY must not exceed 32");

#if TELEMETRY_CHANNEL_CAPACITY == 32U
#define TELEMETRY_CHANNEL_MASK UINT32_MAX
#else
#define TELEMETRY_CHANNEL_MASK ((uint32_t)((1UL << TELEMETRY_CHANNEL_CAPACITY) - 1UL))
#endif

/**
 * @brief 单帧遥测数据样本（time_ms 单位 ms）。
 */
typedef struct telemetry_sample_s {
  float value[TELEMETRY_CHANNEL_CAPACITY];
  uint32_t valid_mask;
  uint32_t time_ms;
} telemetry_sample_t;

/**
 * @brief 遥测提交状态码。
 */
typedef enum dev_telemetry_status_e {
  DEV_TELEMETRY_OK = 0U,
  DEV_TELEMETRY_BUSY,
  DEV_TELEMETRY_ERROR
} dev_telemetry_status_t;

/**
 * @brief 遥测发送统计快照。
 */
typedef struct telemetry_status_s {
  bool is_busy;
  uint32_t sent_frames;
  uint32_t dropped_frames;
  uint32_t error_count;
} telemetry_status_t;

/**
 * @brief 异步遥测发送后端内部状态（供具体传输后端共用，上层勿直接修改）。
 */
typedef struct telemetry_tx_state_s {
  const volatile uint32_t *uart_tx_err_ptr;
  uint32_t last_uart_tx_err;
  volatile bool busy;
  volatile uint32_t sent_frames;
  volatile uint32_t dropped_frames;
  volatile uint32_t error_count;
} telemetry_tx_state_t;

typedef struct telemetry_base_s telemetry_base_t;

/**
 * @brief 具体遥测协议操作虚表（send 须非阻塞）。
 */
typedef struct telemetry_ops_s {
  dev_telemetry_status_t (*send)(telemetry_base_t *telemetry_h, const telemetry_sample_t *sample);
  void (*get_status)(const telemetry_base_t *telemetry_h, telemetry_status_t *status);
} telemetry_ops_t;

/**
 * @brief 遥测多态基类；须作为具体遥测设备结构体首成员。
 */
typedef struct telemetry_base_s {
  const telemetry_ops_t *ops;
  const char *name;
} telemetry_base_t;

/** @brief 绑定遥测名称与静态操作虚表。 */
void Dev_Telemetry_Base_Init(telemetry_base_t *telemetry_h, const char *name,
                             const telemetry_ops_t *ops);

/** @brief 校验通道掩码与有限性后提交非阻塞发送。 */
dev_telemetry_status_t Dev_Telemetry_Base_Send(telemetry_base_t *telemetry_h,
                                               const telemetry_sample_t *sample);

/** @brief 读取具体遥测后端发送状态与丢帧计数快照。 */
void Dev_Telemetry_Base_GetStatus(const telemetry_base_t *telemetry_h, telemetry_status_t *status);

/** @} */

#endif /* LAYERED_CAR_TELEMETRY_BASE_H */
