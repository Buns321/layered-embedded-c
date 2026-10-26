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
 * @brief JoyCon 命令设备抽象对象。
 */
typedef struct joycon_base_s {
  const joycon_ops_t *ops;
  const char         *name;
} joycon_base_t;

/**
 * @brief 初始化 JoyCon 抽象层并绑定静态操作表。
 * @param joycon_h 抽象对象。
 * @param name 调试名称。
 * @param ops 静态操作表，必需成员不能为空。
 * @pre 对象必须由具体 JoyCon 设备持有。
 */
void Dev_JoyCon_Base_Init(
  joycon_base_t *joycon_h,
  const char *name,
  const joycon_ops_t *ops
);

/**
 * @brief 复制最近一帧有效 JoyCon 输入。
 * @param joycon_h 抽象对象。
 * @param input 输出快照。
 * @return 至少收到一帧有效报文时返回 true。
 * @note 并发行为由具体实现定义；上层可在其他任务调用。
 */
bool Dev_JoyCon_Base_GetInput(
  joycon_base_t *joycon_h,
  joycon_input_t *input
);

/**
 * @brief 复制 JoyCon 设备状态快照。
 * @param joycon_h 抽象对象。
 * @param status 输出状态。
 * @note 并发行为由具体实现定义；上层可在其他任务调用。
 */
void Dev_JoyCon_Base_GetStatus(
  joycon_base_t *joycon_h,
  joycon_status_t *status
);

/** @} */

#endif //LAYERED_EMBEDDED_C_JOYCON_BASE_H
