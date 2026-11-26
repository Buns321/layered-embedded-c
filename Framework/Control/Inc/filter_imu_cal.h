/**
 * @file filter_imu_cal.h
 * @brief IMU 六轴零偏与比例校正滤波器（作用于通道 3..5 陀螺 deg/s 与 6..8 加速度 m/s^2）。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，完全静态分配。
 *          支持通过 SYS_SNAPSHOT_READ/WRITE 在运行期无锁/临界区原子更新六轴标定参数，
 *          适用于多任务读写分离场景（控制/服务任务更新配置，输入任务执行滤波）。
 *
 * @addtogroup Framework_Control
 * @{
 */

#ifndef LAYERED_CAR_FILTER_IMU_CAL_H
#define LAYERED_CAR_FILTER_IMU_CAL_H

#include "filter_base.h"
#include <stdint.h>

/**
 * @brief IMU 六轴校正参数（scale 须在 0.1..10，|gyro_bias_dps| <= 720）。
 */
typedef struct filter_imu_cal_cfg_s {
  float accel_offset_mps2[3]; /**< 三轴加速度计零偏（单位：m/s^2，X/Y/Z 须为有限值）。 */
  float accel_scale[3];       /**< 三轴加速度计比例因子（无量纲，范围：[0.1, 10.0]）。 */
  float gyro_bias_dps[3];     /**< 三轴陀螺仪零偏（单位：deg/s，绝对值 <= 720.0）。 */
} filter_imu_cal_cfg_t;

/**
 * @brief IMU 六轴校正滤波器静态对象（base 必须为首成员）。
 */
typedef struct filter_imu_cal_s {
  filter_base_t base;        /**< 多态基类（必须为首成员）。 */
  filter_imu_cal_cfg_t cfg;  /**< 当前生效的六轴标定参数（受快照临界区保护）。 */
  uint32_t corrected_frames; /**< 累计完成校正的帧计数（无量纲，复位时清零）。 */
} filter_imu_cal_t;

/**
 * @brief 初始化六轴校正滤波器；失败时返回错误码且不修改对象。
 * @param[out] filter_h 待初始化的六轴校正滤波器指针（非 NULL）。
 * @param[in]  name     滤波器名称字符串（非 NULL）。
 * @param[in]  cfg      初始标定参数指针（非 NULL）。
 * @return 初始化状态码。
 * @retval FILTER_STATUS_OK               标定参数合法，初始化并清零帧计数成功。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h、name 或 cfg 为 NULL。
 * @retval FILTER_STATUS_INVALID_CONFIG   比例因子或零偏超出合法范围/非有限值。
 */
filter_status_t Filter_ImuCal_Init(filter_imu_cal_t *filter_h, const char *name,
                                   const filter_imu_cal_cfg_t *cfg);

/**
 * @brief 在临界区内原子更新六轴校正参数；非法参数返回错误且保持原配置。
 * @param[in,out] filter_h 已初始化的六轴校正滤波器指针（非 NULL）。
 * @param[in]     cfg      新的标定参数指针（非 NULL）。
 * @return 更新状态码。
 * @retval FILTER_STATUS_OK               新参数已原子写入。
 * @retval FILTER_STATUS_INVALID_ARGUMENT filter_h 或 cfg 为 NULL。
 * @retval FILTER_STATUS_INVALID_CONFIG   新参数越界或非有限，原配置保持不变。
 * @note 内部通过 SYS_SNAPSHOT_WRITE 保护写操作，可安全与滤波任务并发调用。
 */
filter_status_t Filter_ImuCal_SetConfig(filter_imu_cal_t *filter_h,
                                        const filter_imu_cal_cfg_t *cfg);

/**
 * @brief 在临界区内原子读取当前六轴校正参数快照。
 * @param[in]  filter_h 已初始化的六轴校正滤波器指针（为 NULL 时静默返回）。
 * @param[out] cfg      用于接收标定参数快照的目标结构体指针（为 NULL 时静默返回）。
 * @note 内部通过 SYS_SNAPSHOT_READ 保证多字节结构体读取的一致性。
 */
void Filter_ImuCal_GetConfig(const filter_imu_cal_t *filter_h, filter_imu_cal_cfg_t *cfg);

#endif /* LAYERED_CAR_FILTER_IMU_CAL_H */

/** @} */
