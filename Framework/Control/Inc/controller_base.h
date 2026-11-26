/**
 * @file controller_base.h
 * @brief 轮级 SISO 控制律抽象接口与多态基类。
 * @details 本模块属于 Framework/Control 层，100% HAL-Free，严禁包含 BSP、STM32 HAL、
 *          FreeRTOS 或 Core/Inc 头文件。采用静态多态基类模式，所有具体控制律结构体
 *          必须将 controller_base_t 作为首成员并绑定静态只读虚表。
 *
 * @defgroup Framework_Control 控制、滤波与运动学层
 * @{
 */

#ifndef LAYERED_CAR_CONTROLLER_BASE_H
#define LAYERED_CAR_CONTROLLER_BASE_H

#include <stdbool.h>
#include <stdint.h>

/** @brief 控制律多态基类前置声明。 */
typedef struct controller_base_s controller_base_t;

/**
 * @brief 控制律单步执行状态枚举。
 */
typedef enum controller_status_e {
  CONTROLLER_STATUS_OK = 0U,   /**< 正常完成计算，未触发限幅或异常。 */
  CONTROLLER_STATUS_SATURATED, /**< 计算过程触发目标限幅、输出限幅或步长限幅。 */
  CONTROLLER_STATUS_ERROR      /**< 非法参数、非有限输入或时间步长无效，输出已清零。 */
} controller_status_t;

/**
 * @brief 控制律虚表（target/measure 轮速单位 m/s，dt_s 单位 s，out 为 duty）。
 */
typedef struct controller_ops_s {
  /**
   * @brief 单步控制律计算回调。
   * @param[in,out] controller_h 控制律基类句柄（非 NULL）。
   * @param[in]     target       期望目标值（轮速环单位：m/s）。
   * @param[in]     measure      当前测量值（轮速环单位：m/s）。
   * @param[in]     dt_s         控制周期（单位：s，须 > 0）。
   * @param[out]    out          输出控制量指针（单位：duty，非 NULL）。
   * @return 执行状态枚举。
   */
  controller_status_t (*update)(controller_base_t *controller_h, float target, float measure,
                                float dt_s, float *out);
  /**
   * @brief 复位控制律内部运行状态回调。
   * @param[in,out] controller_h 控制律基类句柄（非 NULL）。
   */
  void (*reset)(controller_base_t *controller_h);
} controller_ops_t;

/**
 * @brief 控制律多态基类；须作为具体控制器结构体首成员（禁止按值拷贝）。
 */
typedef struct controller_base_s {
  const controller_ops_t *ops; /**< 指向静态只读控制律虚表的非拥有指针。 */
  const char *name;            /**< 控制律实例名称字符串（生命周期须覆盖实例全程）。 */
} controller_base_t;

/**
 * @brief 绑定控制律名称与静态虚表（仅初始化阶段调用）。
 * @param[out] controller_h 待初始化的控制律基类句柄（为 NULL 时静默返回）。
 * @param[in]  name         控制律名称字符串（非 NULL）。
 * @param[in]  ops          静态只读虚表指针（非 NULL，且 update 与 reset 均非 NULL）。
 * @pre 须由具体控制律 Init 函数在初始化阶段调用，不可在运行期动态改绑。
 * @note 任一入参或虚表回调为 NULL 时不修改 controller_h。
 */
void Ctrl_Base_Init(controller_base_t *controller_h, const char *name, const controller_ops_t *ops);

/**
 * @brief 运行一次控制律计算；非法指针或 dt_s <= 0 时清零 *out 并返回 ERROR。
 * @param[in,out] controller_h 控制律基类句柄（非 NULL，且已绑定有效 ops）。
 * @param[in]     target       期望目标值（轮速环单位：m/s，须为有限值）。
 * @param[in]     measure      当前测量值（轮速环单位：m/s，须为有限值）。
 * @param[in]     dt_s         控制周期（单位：s，须为正有限值 > 0）。
 * @param[out]    out          输出控制量指针（单位：duty，非 NULL）。
 * @return 控制律单步执行状态。
 * @retval CONTROLLER_STATUS_OK        计算成功且未发生限幅。
 * @retval CONTROLLER_STATUS_SATURATED 计算成功但触发了目标/输出/步长限幅。
 * @retval CONTROLLER_STATUS_ERROR     指针无效或 dt_s/输入非法，*out 已安全清零。
 * @pre 须在具体控制律 Init 成功返回后由周期控制任务调用。
 */
controller_status_t Ctrl_Base_Update(controller_base_t *controller_h, float target, float measure,
                                     float dt_s, float *out);

/**
 * @brief 复位控制律内部运行状态（保留配置参数与绑定关系）。
 * @param[in,out] controller_h 控制律基类句柄（为 NULL 或未初始化时静默返回）。
 * @note 用于急停、模式切换或故障恢复时清除积分器与历史状态。
 */
void Ctrl_Base_Reset(controller_base_t *controller_h);

#endif /* LAYERED_CAR_CONTROLLER_BASE_H */

/** @} */
