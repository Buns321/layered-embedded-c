//
// 由 Buns_ 创建于 2026/9/1。
//

/**
 * @file imu_wht101_uart.h
 * @brief WHT101 UART IMU 具体实现。
 * @addtogroup drivers_concrete
 * @{
 */

#ifndef LAYERED_EMBEDDED_C_IMU_WHT101_UART_H
#define LAYERED_EMBEDDED_C_IMU_WHT101_UART_H

#include "imu_base.h"
#include "system_byte_ring.h"
#include "uart_base.h"
#include <stdbool.h>

/**
 * @brief 帧解析状态。
 * @note SYNCING_1 等待 0x55，SYNCING_2 等待类型字节，RECEIVING_DATA 收集数据，
 *       CALCULATING_SUM 校验并提交；状态只在任务侧解析器中推进。
 */
typedef enum imu_wht101_state_e {
  IMU_WHT101_STATE_SYNCING_1 = 0U,
  IMU_WHT101_STATE_SYNCING_2,
  IMU_WHT101_STATE_RECEIVING_DATA,
  IMU_WHT101_STATE_CALCULATING_SUM,
} imu_wht101_state_t;

/**
 * @brief WHT101 寄存器地址。
 * @note 数值为设备协议的寄存器编号，不是内存地址；写入前需先发送 KEY 解锁。
 */
typedef enum imu_wht101_reg_e {
  IMU_WHT101_REG_SAVE    = 0x00,
  IMU_WHT101_REG_RRATE   = 0x03,
  IMU_WHT101_REG_BAUD    = 0x04,
  IMU_WHT101_REG_KEY     = 0x69,
  IMU_WHT101_REG_CALIYAW = 0x76,
  IMU_WHT101_REG_MANCALI = 0xA6,
} imu_wht101_reg_t;

/**
 * @brief 输出速率配置值。
 * @note 数值是写入 RRATE 寄存器的原始配置码，不是以 Hz 为单位的频率。
 */
typedef enum imu_wht101_rate_e {
  IMU_RATE_10HZ  = 0x06,
  IMU_RATE_100HZ = 0x09,
  IMU_RATE_200HZ = 0x0B,
} imu_wht101_rate_t;

/**
 * @brief 非阻塞配置状态机状态。
 * @note IDLE 表示无进行中的设备写入；其余状态等待 config_deadline_ms 后推进，
 *       由 process 在任务上下文单线程驱动。
 */
typedef enum imu_wht101_config_state_e {
  IMU_WHT101_CONFIG_IDLE = 0U,
  IMU_WHT101_CONFIG_ZERO_WAIT_CALIYAW,
  IMU_WHT101_CONFIG_ZERO_WAIT_SAVE,
  IMU_WHT101_CONFIG_RATE_WAIT_WRITE,
  IMU_WHT101_CONFIG_RATE_WAIT_SAVE,
} imu_wht101_config_state_t;

#define IMU_WHT101_SCALE      32768.0f
#define IMU_WHT101_YAW_RANGE  180.0f
#define IMU_WHT101_GYRO_RANGE 2000.0f

#define IMU_WHT101_FRAME_HEADER    0x55
#define IMU_WHT101_FRAME_LEN       11
#define IMU_WHT101_TYPE_GYRO       0x52
#define IMU_WHT101_TYPE_ANGLE      0x53
#define IMU_WHT101_WRITE_HDR1      0xFF
#define IMU_WHT101_WRITE_HDR2      0xAA
#define IMU_WHT101_KEY_UNLOCK      0xB588
#define IMU_WHT101_DELAY_UNLOCK_MS 200
#define IMU_WHT101_DELAY_SAVE_MS   500
#define IMU_WHT101_DELAY_WRITE_MS  100
#define IMU_WHT101_WRITE_LEN       5
#define IMU_WHT101_SAVE_VALUE      0x0000
#define IMU_WHT101_CALIYAW_ZERO    0x0000

#define IMU_WHT101_RX_BUFFER_SIZE 256u

/**
 * @brief WHT101 对象。
 * @note rx_ring 使用 256 字节中断单写、任务单读环形队列，解析在 process 中完成。
 * @note frame、frame_idx、state、config_state 和 pending 标志只允许任务上下文访问。
 * @note 置零和输出速率配置通过 zero_pending / rate_pending 非阻塞推进；同时请求时置零优先。
 * @note uart_h 与 imu_base.filter_h 都是非 owning 指针，生命周期必须覆盖对象和回调注册期。
 * @note 对象不允许复制；复制会切断 ops 回调与具体字段的对应关系。
 */
typedef struct imu_wht101_uart_s {
  imu_base_t imu_base;
  uart_base_t *uart_h;

  imu_wht101_state_t state;
  uint8_t frame[IMU_WHT101_FRAME_LEN];
  uint8_t frame_idx;

  bool is_started;

  imu_wht101_config_state_t config_state;
  uint32_t config_deadline_ms;
  bool zero_pending;
  bool rate_pending;
  volatile bool config_failed;
  uint8_t rate_pending_value;

  uint8_t rx_byte;
  uint8_t rx_buffer[IMU_WHT101_RX_BUFFER_SIZE];
  sys_byte_ring_t rx_ring;

  volatile uint32_t rx_byte_count;
  volatile uint32_t rx_error_count;
  volatile uint32_t rx_checksum_error_count;
  volatile bool rx_resync_requested;
} imu_wht101_uart_t;

/**
 * @brief 初始化 WHT101 对象、解析状态和配置状态。
 * @param imu_h 具体对象；调用方静态分配，函数只初始化已声明字段。
 * @param name 调试名称；只保存指针，其生命周期必须覆盖对象。
 * @param uart_h 已初始化的 UART 后端；只保存非 owning 指针，后端必须支持中断接收。
 * @param filter_h 已初始化的静态滤波器根节点；WHT101 通常注入 passthrough，必须非空。
 * @pre 四个参数非空；uart_h 尚未被本对象占用；对象未被并发访问。
 * @post imu_base 绑定 WHT101 操作表并保存 filter_h；解析/配置/计数状态清零，is_started 为 false。
 * @note 不注册 UART 回调、不启动接收、不写设备寄存器；frame 与 rx_buffer 内容不保证被清零。
 * @note 必须在任务上下文、Start 之前调用；重复初始化会丢弃未完成的配置请求状态。
 * @warning 不分配动态内存；对象、name、uart_h 和 filter_h 的生命周期必须覆盖整个使用期。
 */
void Dev_IMU_WHT101_UART_Init(
  imu_wht101_uart_t *imu_h,
  const char *name,
  uart_base_t *uart_h,
  filter_base_t *filter_h
);
/**
 * @brief 向 WHT101 帧解析状态机输入一个字节。
 * @param imu_h 已 Init 且已 Start 的对象；由 process 或主机测试调用。
 * @param byte 输入字节，取值 0..255；0x55 为帧头，0x52/0x53 为已知类型。
 * @param now_ms 当前单调毫秒时间戳，作为成功提交帧的 time_ms。
 * @pre imu_h 非空且已 Start；对象只允许一个任务写；不得在 ISR 中调用。
 * @post 11 字节帧通过累加校验后提交相应通道；校验失败递增 rx_checksum_error_count；
 *       未知类型或同步错误丢弃当前帧并回到等待帧头。
 * @note 失败不返回错误码；提交失败由 imu_base 内部断言/过滤处理，本函数不重试。
 * @warning 必须在 Start 之后调用，否则 Dev_IMU_Base_SubmitSample 断言失败。
 */
void Dev_IMU_WHT101_UART_FeedByte      (imu_wht101_uart_t *imu_h, uint8_t byte, uint32_t now_ms);
/**
 * @brief 请求设备侧设置 yaw 零点。
 * @param imu_h 已 Start 的对象；请求只置位 pending 标志。
 * @pre imu_h 非空且 is_started 为 true；只能由单一任务调用，不得与 process 并发。
 * @post zero_pending 置 true，config_failed 清零；实际“解锁、CALIYAW、保存”由后续 process 推进。
 * @note 非阻塞，不等待设备应答，也不改变 imu_base 的 yaw 偏移；成功/失败通过状态快照的 is_busy 与 last_request_failed 观察。
 * @warning 若已有配置事务进行中，本请求会等到该事务空闲后执行；不会取消正在进行的写入。
 */
void Dev_IMU_WHT101_UART_SetZero       (imu_wht101_uart_t *imu_h);
/**
 * @brief 请求设置设备输出速率。
 * @param imu_h 已 Start 的对象；请求只缓存速率并置位 pending 标志。
 * @param rate RRATE 寄存器原始配置码，典型值为 IMU_RATE_10HZ / 100HZ / 200HZ；
 *             本函数不校验取值，非法码由设备决定是否接受。
 * @pre imu_h 非空且 is_started 为 true；只能由单一任务调用，不得与 process 并发。
 * @post rate_pending_value 保存 rate，rate_pending 置 true，config_failed 清零；
 *       实际“解锁、RRATE、保存”由后续 process 推进。
 * @note 非阻塞；若置零请求同时存在，置零事务先完成；成功/失败通过状态快照观察。
 * @warning 请求不排队多次：新的调用会覆盖尚未处理的 rate_pending_value。
 */
void Dev_IMU_WHT101_UART_SetOutputRate (imu_wht101_uart_t *imu_h, uint8_t rate);

/** @} */

#endif //LAYERED_EMBEDDED_C_IMU_WHT101_UART_H
