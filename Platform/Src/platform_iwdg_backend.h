/**
 * @file platform_iwdg_backend.h
 * @brief STM32 系列公共 IWDG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H

#include "platform_def.h"
#include "platform_iwdg.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 返回后端用于 IWDG 时基计算的 LSI 标称频率。
 * @return LSI 标称频率，单位 Hz；F1 为 40000，F4 为 32000。
 * @note 该值是编译期标称值，不读取芯片校准数据；实际 LSI 偏差会反映到看门狗超时。
 * @note 纯读取，不访问硬件、不分配内存，可在任务或中断上下文调用。
 */
uint32_t platform_iwdg_backend_lsi_hz(void);

/**
 * @brief 配置并启动 IWDG 后端。
 * @param divisor 预分频除数，无量纲；只接受 4、8、16、32、64、128 或 256。
 * @param reload 12 位重装计数值，范围 0..0x0FFF；本函数不夹紧该值。
 * @retval PLATFORM_STATUS_OK IWDG 已启动。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT divisor 不受支持。
 * @retval PLATFORM_STATUS_TIMEOUT HAL 等待预分频或重装寄存器更新超时。
 * @pre 调用方已完成时钟和寄存器访问准备；本函数不执行 CubeMX 结构性初始化。
 * @post 成功后 IWDG 开始计数且无法停止；HAL 启动后发生的失败不会回滚，仍可能继续计数并复位。
 * @warning 这是运行期不可逆操作，重复启动或配置错误可能立即触发复位。
 * @note 仅由 Platform 公共层按单所有者策略调用；不分配内存，不加锁，不支持并发调用。
 */
platform_status_t platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
);

/**
 * @brief 刷新已启动的 IWDG 计数器。
 * @retval PLATFORM_STATUS_OK HAL 已接受重装命令。
 * @pre 已成功调用 platform_iwdg_backend_start()；本函数不检查启动状态。
 * @post 成功时计数器从重装值继续递减。
 * @warning 必须在超时前调用；过晚调用会导致芯片复位。
 * @note 当前 HAL 刷新路径恒返回成功；不分配内存、无锁，调用方必须串行化并发刷新。
 */
platform_status_t platform_iwdg_backend_refresh(void);

/**
 * @brief 读取并清除本次 IWDG 复位标志。
 * @retval true RCC 复位状态包含 IWDG 标志，并已清除复位标志锁存。
 * @retval false 未观察到 IWDG 复位标志，不修改寄存器。
 * @post true 分支写入 RMVF，清除全部共享的 STM32 复位标志。
 * @warning 清除动作影响所有复位源，其他模块随后可能无法再读取本次复位原因。
 * @note 直接访问 RCC->CSR；不分配内存，读取与清除不构成原子操作，需由调用方串行化。
 */
bool platform_iwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_IWDG_BACKEND_H
