/**
 * @file imu_wht101.h
 * @brief WHT101 串口陀螺仪驱动接口。
 * @details 属于 Framework/Drivers 设备驱动层，直连 bsp_uart_t（支持字节中断或
 *          DMA+IDLE 接收），解析维特智能 11 字节二进制帧（角速度 0x52 与角度 0x53），
 *          并内置非阻塞寄存器写状态机与可选上电静止自动归零状态机。
 *
 * @addtogroup Framework_Drivers
 * @{
 */

#ifndef LAYERED_CAR_IMU_WHT101_H
#define LAYERED_CAR_IMU_WHT101_H

#include "bsp_def.h"
#include "bsp_uart.h"
#include "imu_base.h"
#include <stdbool.h>
#include <stdint.h>

/** @brief WHT101 单帧二进制协议固定长度（单位：字节）。 */
#define IMU_WHT101_FRAME_LEN 11U
#ifndef IMU_WHT101_RX_RING_SIZE
/** @brief WHT101 串口接收环形缓冲区容量（单位：字节，须为 2 的幂）。 */
#define IMU_WHT101_RX_RING_SIZE 128U
#endif
#ifndef IMU_WHT101_RX_DMA_SIZE
/** @brief WHT101 DMA+IDLE 模式单次接收暂存缓冲区容量（单位：字节）。 */
#define IMU_WHT101_RX_DMA_SIZE 32U
#endif

/**
 * @brief WHT101 输出速率寄存器（RRATE 0x03）编码枚举。
 */
typedef enum imu_wht101_rate_e {
  IMU_WHT101_RATE_10HZ = 0x06U,  /**< 10 Hz 输出速率 */
  IMU_WHT101_RATE_100HZ = 0x09U, /**< 100 Hz 输出速率 */
  IMU_WHT101_RATE_200HZ = 0x0BU  /**< 200 Hz 输出速率 */
} imu_wht101_rate_t;

/**
 * @brief 上电自动归零状态机阶段枚举。
 */
typedef enum imu_wht101_boot_state_e {
  IMU_WHT101_BOOT_IDLE = 0U,       /**< 尚未启动 */
  IMU_WHT101_BOOT_RATE_CONFIG,     /**< 正在下发输出速率配置序列 */
  IMU_WHT101_BOOT_WAIT_STILL,      /**< 等待 Z 轴角速度持续静止达标 */
  IMU_WHT101_BOOT_ZERO_CONFIG,     /**< 正在下发航向角归零（CALIYAW）指令序列 */
  IMU_WHT101_BOOT_WAIT_ZERO_FRAME, /**< 等待归零后的新角度帧残差收敛 */
  IMU_WHT101_BOOT_READY,           /**< 初始化与归零全部完成，可投入闭环使用 */
  IMU_WHT101_BOOT_FAULT            /**< 配置发送失败、静止超时或归零校验超时故障态 */
} imu_wht101_boot_state_t;

/**
 * @brief WHT101 初始化配置结构体。
 * @details 数值为 0 的超时与阈值字段将自动回退为安全默认参数。
 */
typedef struct imu_wht101_cfg_s {
  const char *name;            /**< 调试名称字符串（非 NULL） */
  UART_HandleTypeDef *huart;   /**< STM32 HAL UART 句柄指针（非 NULL） */
  bsp_uart_rx_mode_t rx_mode;  /**< 串口接收模式（NONE 时默认按 BSP_UART_RX_IT_BYTE 初始化） */
  filter_base_t *filter;       /**< 绑定的 9 通道滤波器句柄（非 NULL） */
  bool auto_zero_on_boot;      /**< 是否启用上电自动速率配置、静止检测与航向归零 */
  imu_wht101_rate_t boot_rate; /**< 上电目标输出速率（0 时默认 IMU_WHT101_RATE_10HZ） */
  uint32_t cfg_timeout_ms;     /**< 单次寄存器配置阶段超时（单位：ms，0 默认 3000 ms） */
  uint32_t still_timeout_ms;   /**< 等待静止总超时（单位：ms，0 默认 5000 ms） */
  uint32_t still_confirm_ms;   /**< 连续满足静止条件的确认窗口（单位：ms，0 默认 400 ms） */
  float still_gyro_dps;     /**< 静止判定 Z 轴角速度绝对值阈值（单位：deg/s，0 默认 3.0 deg/s） */
  uint32_t zero_timeout_ms; /**< 归零后等待首帧角度收敛超时（单位：ms，0 默认 2000 ms） */
  float zero_residual_deg;  /**< 归零后允许的最大残差偏航角（单位：deg，0 默认 5.0 deg） */
} imu_wht101_cfg_t;

/**
 * @brief WHT101 串口字节流帧同步解析状态机枚举。
 */
typedef enum imu_wht101_rx_state_e {
  IMU_WHT101_RX_SYNC_1 = 0U, /**< 等待帧头字节 0x55 */
  IMU_WHT101_RX_SYNC_2,      /**< 等待帧类型字节（0x52 角速度 / 0x53 角度） */
  IMU_WHT101_RX_DATA,        /**< 接收 8 字节载荷数据（索引 2..9） */
  IMU_WHT101_RX_CHECKSUM     /**< 接收并校验第 11 字节累加和 */
} imu_wht101_rx_state_t;

/**
 * @brief WHT101 非阻塞寄存器写指令状态机枚举。
 */
typedef enum imu_wht101_cfg_state_e {
  IMU_WHT101_CFG_IDLE = 0U,         /**< 空闲态，可接受新的配置请求 */
  IMU_WHT101_CFG_ZERO_WAIT_CALIYAW, /**< 已发送解锁 Key，等待 200 ms 后写 CALIYAW */
  IMU_WHT101_CFG_ZERO_WAIT_SAVE,    /**< 已发送 CALIYAW，等待 500 ms 后写 SAVE */
  IMU_WHT101_CFG_RATE_WAIT_WRITE,   /**< 已发送解锁 Key，等待 200 ms 后写 RRATE */
  IMU_WHT101_CFG_RATE_WAIT_SAVE     /**< 已发送 RRATE，等待 100 ms 后写 SAVE */
} imu_wht101_cfg_state_t;

/**
 * @brief WHT101 静态驱动对象结构体。
 * @note base 必须作为首成员以支持 imu_base_t* 多态转换；对象内含环形缓冲区，禁止按值拷贝。
 */
typedef struct imu_wht101_s {
  imu_base_t base;                              /**< IMU 多态基类（必须为首成员） */
  bsp_uart_t uart;                              /**< 底层串口接收与阻塞发送实例 */
  uint8_t rx_ring_buf[IMU_WHT101_RX_RING_SIZE]; /**< ISR 写入、任务读出的 SPSC 接收环形缓冲 */
  uint8_t rx_dma_buf[IMU_WHT101_RX_DMA_SIZE];   /**< DMA+IDLE 模式硬件接收缓冲区 */
  imu_wht101_rx_state_t rx_state;               /**< 当前字节流解析状态 */
  uint8_t frame[IMU_WHT101_FRAME_LEN];          /**< 正在组装的 11 字节协议帧缓存 */
  uint8_t frame_idx;                            /**< 当前帧已接收字节索引（0..10） */
  bool is_started;                              /**< 启动标志 */
  imu_wht101_cfg_state_t cfg_state;             /**< 当前非阻塞写寄存器序列阶段 */
  uint32_t cfg_deadline_ms;                     /**< 下一条写命令触发截止时间戳（单位：ms） */
  bool zero_pending;                            /**< 挂起的硬件归零请求标志 */
  bool rate_pending;                            /**< 挂起的输出速率修改请求标志 */
  uint8_t rate_pending_value;                   /**< 待写入的速率寄存器码 */
  volatile bool config_failed;                  /**< 最近一次串口写命令是否失败 */
  bool auto_zero_on_boot;                       /**< 是否启用上电自动归零流程 */
  bool boot_started_pending;                    /**< 首次 process 对齐启动时间戳的延迟标记 */
  imu_wht101_rate_t boot_rate;                  /**< 上电目标输出速率 */
  imu_wht101_boot_state_t boot_state;           /**< 上电自动归零状态机当前阶段 */
  uint32_t boot_state_started_ms;               /**< 当前 boot_state 进入时间戳（单位：ms） */
  uint32_t config_done_ms;          /**< 速率配置完成时间戳，用于过滤旧样本（单位：ms） */
  uint32_t still_since_ms;          /**< 连续静止起始时间戳（单位：ms） */
  bool still_tracking;              /**< 是否正在累计连续静止时长 */
  uint32_t zero_done_ms;            /**< 归零指令发送完成时间戳，用于过滤旧帧（单位：ms） */
  uint32_t cfg_timeout_ms;          /**< 配置超时阈值（单位：ms） */
  uint32_t still_timeout_ms;        /**< 等待静止超时阈值（单位：ms） */
  uint32_t still_confirm_ms;        /**< 静止确认窗口时长（单位：ms） */
  float still_gyro_dps;             /**< 静止角速度阈值（单位：deg/s） */
  uint32_t zero_timeout_ms;         /**< 归零收敛超时阈值（单位：ms） */
  float zero_residual_deg;          /**< 归零残差阈值（单位：deg） */
  volatile uint32_t tx_error_count; /**< 串口写命令发送失败计数 */
  volatile uint32_t rx_checksum_error_count; /**< 接收帧累加和校验失败计数 */
} imu_wht101_t;

/**
 * @brief 初始化 WHT101 与内部静态 UART 接收通道。
 * @param[out] imu WHT101 静态对象指针（非 NULL）。
 * @param[in]  cfg 初始化配置结构体指针（非 NULL）。
 * @return 初始化状态码。
 * @retval BSP_OK              串口与基类初始化成功（尚未启动接收）。
 * @retval BSP_ERR_INVALID_ARG 参数为空、接收模式非法或浮点阈值为非有限/非正值。
 * @retval 其他 bsp_status_t   底层 BSP_UART_Init 返回的错误码。
 * @pre 对应 UART 外设（及 DMA，若使用 DMA_IDLE）已完成 CubeMX 初始化。
 */
bsp_status_t Dev_IMU_WHT101_Init(imu_wht101_t *imu, const imu_wht101_cfg_t *cfg);

/**
 * @brief 向帧解析状态机喂入单字节。
 * @param[in,out] imu    WHT101 对象指针（为 NULL 时安全忽略）。
 * @param[in]     byte   新接收到的串口字节。
 * @param[in]     now_ms 当前系统单调毫秒时间戳（单位：ms）。
 * @pre 仅在任务上下文、已调用 Dev_IMU_Base_Start() 后调用。
 */
void Dev_IMU_WHT101_FeedByte(imu_wht101_t *imu, uint8_t byte, uint32_t now_ms);

/**
 * @brief 发起硬件偏航角归零命令序列。
 * @details 本函数仅置位挂起标志，实际解锁、写 CALIYAW 与写 SAVE 由后续
 *          Dev_IMU_Base_Process() 非阻塞分步完成。
 * @param[in,out] imu WHT101 对象指针（为 NULL 或未启动时安全忽略）。
 */
void Dev_IMU_WHT101_SetZero(imu_wht101_t *imu);

/**
 * @brief 发起输出速率配置命令序列。
 * @details 本函数仅缓存目标速率并置位挂起标志，由后续 process 非阻塞完成解锁、写入与保存。
 * @param[in,out] imu  WHT101 对象指针（为 NULL 或未启动时安全忽略）。
 * @param[in]     rate 目标输出速率寄存器码（参见 imu_wht101_rate_t）。
 */
void Dev_IMU_WHT101_SetOutputRate(imu_wht101_t *imu, uint8_t rate);

/**
 * @brief 查询上电自动归零状态机当前阶段。
 * @param[in] imu WHT101 对象指针（允许为 NULL）。
 * @return 当前启动状态枚举值（imu 为 NULL 时返回 IMU_WHT101_BOOT_FAULT）。
 */
imu_wht101_boot_state_t Dev_IMU_WHT101_GetBootState(const imu_wht101_t *imu);

/**
 * @brief 查询 WHT101 是否已启动且完成初始化（READY）。
 * @param[in] imu WHT101 对象指针（允许为 NULL）。
 * @return 是否就绪。
 * @retval true  已启动且 boot_state 为 IMU_WHT101_BOOT_READY。
 * @retval false 指针为空、未启动或尚在归零/故障状态。
 */
bool Dev_IMU_WHT101_IsReady(const imu_wht101_t *imu);

/**
 * @brief 查询上电自动归零状态机是否进入故障状态。
 * @param[in] imu WHT101 对象指针（允许为 NULL）。
 * @return 是否处于故障态。
 * @retval true  指针为空或 boot_state 为 IMU_WHT101_BOOT_FAULT。
 * @retval false 未发生启动故障。
 */
bool Dev_IMU_WHT101_HasFault(const imu_wht101_t *imu);

#endif /* LAYERED_CAR_IMU_WHT101_H */

/** @} */
