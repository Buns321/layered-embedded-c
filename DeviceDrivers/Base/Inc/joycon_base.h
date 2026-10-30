//
// 由 Buns_ 创建于 2026/10/26。
//

/**
 * @file joycon_base.h
 * @brief JoyCon 命令设备抽象接口。
 * @addtogroup drivers_base
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_JOYCON_BASE_H
#define LAYERED_EMBEDDED_C_JOYCON_BASE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 一帧 JoyCon 控制输入。
 * @note 四个摇杆量均已归一化到 [-1, 1]。
 * @note right_y 按协议解析并保存，但当前语义为保留字段。
 * @note time_ms 是设备层完成解析的单调毫秒时间戳。
 */
typedef struct joycon_input_s {
  float left_x;
  float left_y;
  float right_x;
  float right_y;
  uint32_t time_ms;
  uint32_t sequence;
} joycon_input_t;

/**
 * @brief JoyCon 设备运行状态快照。
 */
typedef struct joycon_status_s {
  bool     is_started;
  uint32_t received_bytes;
  uint32_t parsed_frames;
  uint32_t invalid_frames;
  uint32_t rx_overflow_count;
  uint32_t uart_error_count;
  uint32_t rearm_error_count;
  uint32_t last_frame_ms;
  uint32_t sequence;
} joycon_status_t;

typedef struct joycon_base_s joycon_base_t;

/**
 * @brief JoyCon 抽象操作表。
 */
typedef struct joycon_ops_s {
  bool (*get_input)(
    joycon_base_t *joycon_h,
    joycon_input_t *input
  );
  void (*get_status)(
    joycon_base_t *joycon_h,
    joycon_status_t *status
  );
} joycon_ops_t;

/**
 * @brief JoyCon 命令设备抽象基类。
 */
typedef struct joycon_base_s {
  const joycon_ops_t *ops;
  const char         *name;
} joycon_base_t;

/**
 * @brief 绑定 JoyCon 名称与只读查询操作表。
 * @param joycon_h 调用方持有的 JoyCon 基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖设备对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，get_input 与 get_status 必须可用。
 * @pre 设备尚未并发发布，name 与 ops 在设备使用期间保持有效。
 * @post 只保存绑定，不启动 UART、不解析报文，也不改变最近输入。
 * @note 仅限初始化阶段调用；不加锁且不分配内存，重复调用会覆盖已有绑定。
 */
void Dev_JoyCon_Base_Init(
  joycon_base_t *joycon_h,
  const char *name,
  const joycon_ops_t *ops
);

/**
 * @brief 复制最近一帧 JoyCon 控制输入。
 * @param joycon_h 已初始化的 JoyCon 句柄；不可为 NULL，所有权不转移。
 * @param input 调用方持有的输出快照；不可为 NULL，函数覆盖其全部字段。
 * @retval true 至少发布过一帧有效报文，input 含最新快照。
 * @retval false 尚无有效报文；input 仍被覆盖为当前存储值，序列号通常为 0。
 * @pre 已绑定非空 get_input 操作；具体实现负责输入快照的并发保护。
 * @post 四轴范围为 [-1, 1]，time_ms 为解析时刻的单调毫秒时间戳。
 * @note 上层可在其他任务读取；复制是快照语义，不转移任何缓冲区所有权。
 */
bool Dev_JoyCon_Base_GetInput(
  joycon_base_t *joycon_h,
  joycon_input_t *input
);

/**
 * @brief 复制 JoyCon 设备运行状态。
 * @param joycon_h 已初始化的 JoyCon 句柄；不可为 NULL，所有权不转移。
 * @param status 调用方持有的输出状态；不可为 NULL，函数覆盖其全部字段。
 * @pre 已绑定非空 get_status 操作；具体实现负责统计值的并发读取。
 * @post status 包含启动、收发、解析、错误和最近帧时间统计。
 * @note 字节数与帧数单位分别为 bytes 和帧；时间字段为单调毫秒并允许回绕。
 * @warning 本层不加锁；是否可与其他任务并发调用取决于具体实现。
 */
void Dev_JoyCon_Base_GetStatus(
  joycon_base_t *joycon_h,
  joycon_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JOYCON_BASE_H
