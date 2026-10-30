/**
 * @file platform_wwdg_backend.h
 * @brief STM32 系列公共 WWDG 实现的私有后端接口。
 * @ingroup platform
 */

#ifndef LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
#define LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H

#include "platform_def.h"
#include "platform_wwdg.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 查询窗口看门狗计数所使用的 APB1 时钟频率。
 * @return HAL 当前报告的 PCLK1 频率，单位 Hz；时钟尚未初始化时可能为 0。
 * @post 只读 RCC 时钟配置，不修改 WWDG 或时钟树状态。
 * @note 可在任务或初始化上下文调用；调用方在使用返回值计算超时前必须排除 0 Hz。
 */
uint32_t platform_wwdg_backend_pclk1_hz(void);

/**
 * @brief 配置并立即启动 WWDG 硬件。
 * @param divisor WWDG 预分频除数；仅接受 1、2、4、8。
 * @param window 原始刷新窗口值；公共策略层限定为 PLATFORM_WWDG_WINDOW_MIN..MAX。
 * @retval PLATFORM_STATUS_OK 时钟已使能，WWDG 已启动且开始倒计数。
 * @pre 调用方已完成公共配置校验，并保证同一实例此前未启动；不得与刷新并发。
 * @post 除数合法时会先使能 WWDG 时钟并覆盖本文件静态 HAL 句柄配置；非法除数在触硬件前返回。
 * @warning 启动后无法停止；若未在窗口内刷新，硬件将在计数到达 0x3F 时复位 MCU。
 * @note 这是运行期不可逆操作，禁止在普通 ISR 中调用，且调用方必须立即安排首次刷新。
 */
platform_status_t platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
);

/**
 * @brief 刷新已启动的 WWDG 计数器。
 * @retval PLATFORM_STATUS_OK 刷新命令已被硬件接受，计数器重新装载为 0x7F。
 * @pre WWDG 已由本后端成功启动；当前计数器必须低于配置窗口，否则过早刷新会触发复位。
 * @post 成功时计数器重装；失败时计数继续运行，调用方必须立即处理以避免超时复位。
 * @warning 本函数不检查当前计数值是否位于窗口内，调用时机由上层严格保证。
 * @note 可在任务上下文调用；若在 ISR 中调用，必须确保其优先级和抖动不会越过刷新窗口。
 */
platform_status_t platform_wwdg_backend_refresh(void);

/**
 * @brief 读取 WWDG 复位标志，并在命中时清除 RCC 复位锁存。
 * @retval true RCC_CSR 中的 WWDGRSTF 已置位，本次复位由 WWDG 触发。
 * @retval false WWDGRSTF 未置位，本次复位不是由 WWDG 触发。
 * @pre 应在启动阶段、其他代码读取复位原因之前调用；不要求 WWDG 当前已启动。
 * @post 返回 true 时写 RMVF 清除共享复位标志锁存；返回 false 时不清除任何标志。
 * @warning 清除复位标志会同时丢失其他 RCC 复位原因，调用方必须先完成所需诊断。
 * @note 读写共享 RCC->CSR，非原子且无锁；只应由启动流程的单一线程调用。
 */
bool platform_wwdg_backend_was_reset(void);

#endif //LAYERED_EMBEDDED_C_PLATFORM_WWDG_BACKEND_H
