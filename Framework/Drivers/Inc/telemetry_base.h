/**
 * @file telemetry_base.h
 * @brief 协议无关遥测设备多态基类接口。
 * @details 属于 Framework/Drivers 设备驱动层，为 JustFloat、FireWater 等
 *          遥测协议后端提供非阻塞整帧提交/忙时丢弃语义、通道有限性校验以及
 *          共享异步 UART DMA 发送状态结构体。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_TELEMETRY_BASE_H
#define LAYERED_CAR_TELEMETRY_BASE_H

#include <stdbool.h>
#include <stdint.h>

#ifndef TELEMETRY_CHANNEL_CAPACITY
/** @brief 单帧遥测支持的最大通道容量（须满足 1..32）。 */
#define TELEMETRY_CHANNEL_CAPACITY 16U
#endif

_Static_assert(TELEMETRY_CHANNEL_CAPACITY > 0U, "TELEMETRY_CHANNEL_CAPACITY must be positive");
_Static_assert(TELEMETRY_CHANNEL_CAPACITY <= 32U, "TELEMETRY_CHANNEL_CAPACITY must not exceed 32");

#if TELEMETRY_CHANNEL_CAPACITY == 32U
/** @brief 全部有效遥测通道的 32 位全掩码。 */
#define TELEMETRY_CHANNEL_MASK UINT32_MAX
#else
/** @brief 全部有效遥测通道的位掩码（低 TELEMETRY_CHANNEL_CAPACITY 位置 1）。 */
#define TELEMETRY_CHANNEL_MASK ((uint32_t)((1UL << TELEMETRY_CHANNEL_CAPACITY) - 1UL))
#endif

/**
 * @brief 单帧遥测数据样本结构体。
 */
typedef struct telemetry_sample_s {
  float value[TELEMETRY_CHANNEL_CAPACITY]; /**< 各通道浮点数值数组（单位随具体业务通道定义） */
  uint32_t valid_mask; /**< 有效通道位掩码（仅允许置位 TELEMETRY_CHANNEL_MASK 范围内位） */
  uint32_t time_ms;    /**< 样本采集单调毫秒时间戳（单位：ms） */
} telemetry_sample_t;

/**
 * @brief 遥测帧提交状态码枚举。
 */
typedef enum dev_telemetry_status_e {
  DEV_TELEMETRY_OK = 0U, /**< 帧已成功编码并提交异步发送 */
  DEV_TELEMETRY_BUSY,    /**< 上一帧 DMA 发送尚未完成，本帧已主动丢弃 */
  DEV_TELEMETRY_ERROR    /**< 参数非法、含非有限值、缓冲区溢出或底层发送错误 */
} dev_telemetry_status_t;

/**
 * @brief 遥测发送统计快照结构体。
 */
typedef struct telemetry_status_s {
  bool is_busy;            /**< 当前发送通道是否正忙 */
  uint32_t sent_frames;    /**< 已成功发送完成的帧总数 */
  uint32_t dropped_frames; /**< 因忙而主动丢弃的帧总数 */
  uint32_t error_count;    /**< 编码或底层发送累计错误次数 */
} telemetry_status_t;

/**
 * @brief 异步遥测发送后端内部状态结构体。
 * @note 供具体传输后端（telem_uart_tx）共用，ISR 回调与任务上下文共享访问，上层勿直接修改。
 */
typedef struct telemetry_tx_state_s {
  const volatile uint32_t *uart_tx_err_ptr; /**< 指向 bsp_uart_t::tx_error_count
                                               的只读指针，用于检测 ISR 期间是否新增发送错误 */
  uint32_t last_uart_tx_err;                /**< 上一次检查时记录的 UART 发送错误计数快照 */
  volatile bool busy;            /**< 软件发送忙标志（提交 DMA 前置 true，TX 完成回调清 false） */
  volatile uint32_t sent_frames; /**< 累计发送成功帧数 */
  volatile uint32_t dropped_frames; /**< 累计忙丢帧数 */
  volatile uint32_t error_count;    /**< 累计发送/编码错误数 */
} telemetry_tx_state_t;

/** @brief 遥测设备多态基类前向声明。 */
typedef struct telemetry_base_s telemetry_base_t;

/**
 * @brief 具体遥测协议只读虚表（多态操作接口）。
 * @details send 回调必须是非阻塞的，忙时应立即返回 DEV_TELEMETRY_BUSY。
 */
typedef struct telemetry_ops_s {
  dev_telemetry_status_t (*send)(telemetry_base_t *telemetry_h,
                                 const telemetry_sample_t *sample); /**< 编码并提交非阻塞发送 */
  void (*get_status)(const telemetry_base_t *telemetry_h,
                     telemetry_status_t *status); /**< 读取当前发送忙闲与统计快照 */
} telemetry_ops_t;

/**
 * @brief 遥测多态基类结构体。
 * @note 必须作为具体遥测设备结构体的第一个成员，禁止按值拷贝。
 */
typedef struct telemetry_base_s {
  const telemetry_ops_t *ops; /**< 指向具体协议只读虚表的指针 */
  const char *name;           /**< 遥测实例调试名称字符串 */
} telemetry_base_t;

/**
 * @brief 绑定遥测名称与静态操作虚表。
 * @param[out] telemetry_h 遥测基类指针（为 NULL 时安全忽略）。
 * @param[in]  name        调试名称字符串（非 NULL）。
 * @param[in]  ops         只读虚表指针（send 与 get_status 均须非 NULL）。
 */
void Dev_Telemetry_Base_Init(telemetry_base_t *telemetry_h, const char *name,
                             const telemetry_ops_t *ops);

/**
 * @brief 校验通道掩码与有限性后提交非阻塞遥测发送。
 * @param[in,out] telemetry_h 遥测基类指针（非 NULL）。
 * @param[in]     sample      待发送的遥测数据样本（非 NULL，所有置位通道须为有限浮点数）。
 * @return 提交状态码。
 * @retval DEV_TELEMETRY_OK    已成功提交底层非阻塞发送。
 * @retval DEV_TELEMETRY_BUSY  发送通道正忙，本帧被丢弃。
 * @retval DEV_TELEMETRY_ERROR 句柄/样本为空、掩码越界、含 NaN/Inf 或底层发送失败。
 */
dev_telemetry_status_t Dev_Telemetry_Base_Send(telemetry_base_t *telemetry_h,
                                               const telemetry_sample_t *sample);

/**
 * @brief 读取具体遥测后端发送状态与丢帧计数快照。
 * @param[in]  telemetry_h 遥测基类指针（为 NULL 时安全忽略）。
 * @param[out] status      输出状态结构体指针（为 NULL 时安全忽略）。
 */
void Dev_Telemetry_Base_GetStatus(const telemetry_base_t *telemetry_h, telemetry_status_t *status);

#endif /* LAYERED_CAR_TELEMETRY_BASE_H */

/** @} */
