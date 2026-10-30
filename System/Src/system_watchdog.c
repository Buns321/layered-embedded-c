//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_watchdog.c
 * @brief System 看门狗策略实现。
 * @ingroup system
 */

#include "system_watchdog.h"
#include "platform_iwdg.h"
#include "system_critical.h"
#include <assert.h>

static uint32_t s_timeout_ms;
static uint32_t s_last_checkin[SYS_WATCHDOG_TASK_COUNT];
static uint32_t s_checkin_mask;

/**
 * @brief 启动 IWDG。
 * @param timeout_ms 目标超时（ms），内部夹紧到 1..PLATFORM_IWDG_MAX_TIMEOUT_MS。
 * @pre Platform 已初始化，且监控任务和所有报到任务即将可用。
 * @note 应在任务启动前调用，避免与 Sys_Watchdog_CheckIn 并发。
 * @warning IWDG 一旦启动不能停止；实际超时受 LSI 频率偏差影响。启动失败只在
 *          Debug 构建触发 assert，Release 构建不会向调用方返回错误。
 * @post 超时值已夹紧并保存，报到时间戳和报到位图已清零，IWDG 已按目标超时启动。
 */
void Sys_Watchdog_Init(uint32_t timeout_ms) {
  assert(timeout_ms > 0u);

  if (timeout_ms == 0u) {
    timeout_ms = 1u;
  }

  if (timeout_ms > PLATFORM_IWDG_MAX_TIMEOUT_MS) {
    timeout_ms = PLATFORM_IWDG_MAX_TIMEOUT_MS;
  }

  s_timeout_ms = timeout_ms;

  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    s_last_checkin[i] = 0u;
  }

  s_checkin_mask = 0u;

  platform_status_t status = Platform_IWDG_Start(
    PLATFORM_IWDG_1,
    timeout_ms
  );
  assert(status == PLATFORM_STATUS_OK);
}

/**
 * @brief 任务报到。
 * @param task 报到任务标识，必须小于 SYS_WATCHDOG_TASK_COUNT。
 * @param now_ms 当前单调毫秒时间戳，单位 ms，允许 32 位回绕。
 * @pre 在任务上下文调用，且 task 在枚举有效范围内。
 * @post 该任务的时间戳已更新、报到位已置位；写入在临界区内完成。
 * @warning 不得在 ISR 中调用；非法 task 只在 Debug 构建触发 assert，Release 构建忽略本次报到。
 */
void Sys_Watchdog_CheckIn(sys_watchdog_task_t task, uint32_t now_ms) {
  assert((uint32_t)task < (uint32_t)SYS_WATCHDOG_TASK_COUNT);

  if ((uint32_t)task < (uint32_t)SYS_WATCHDOG_TASK_COUNT) {
    Sys_Critical_Enter();
    s_last_checkin[task] = now_ms;
    s_checkin_mask |= (uint32_t)1u << (uint32_t)task;
    Sys_Critical_Exit();
  }
}

/**
 * @brief 检查所有任务是否新鲜；全部新鲜时喂狗。
 * @param now_ms 当前单调毫秒时间戳，单位 ms；使用无符号差计算任务年龄。
 * @retval true 所有任务均已报到、年龄未超过超时值，且 IWDG 刷新成功。
 * @retval false 存在未报到或超时任务，或 Platform_IWDG_Refresh 失败。
 * @pre Sys_Watchdog_Init 已调用，且本函数在监控任务上下文运行。
 * @post 返回 true 时已刷新 IWDG；返回 false 时不刷新，并保持各任务报到状态不变。
 * @note 不要在 ISR 或其他驱动中直接喂狗；任务年龄按 32 位无符号回绕语义计算。
 */
bool Sys_Watchdog_Service(uint32_t now_ms) {
  const uint32_t all_tasks_mask =
    ((uint32_t)1u << (uint32_t)SYS_WATCHDOG_TASK_COUNT) - 1u;

  uint32_t checkin_mask;
  uint32_t last_checkin[SYS_WATCHDOG_TASK_COUNT];

  Sys_Critical_Enter();
  checkin_mask = s_checkin_mask;
  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    last_checkin[i] = s_last_checkin[i];
  }
  Sys_Critical_Exit();

  if ((checkin_mask & all_tasks_mask) != all_tasks_mask) {
    return false;
  }

  for (uint32_t i = 0u; i < (uint32_t)SYS_WATCHDOG_TASK_COUNT; i++) {
    if ((uint32_t)(now_ms - last_checkin[i]) > s_timeout_ms) {
      return false;
    }
  }

  return Platform_IWDG_Refresh(PLATFORM_IWDG_1) == PLATFORM_STATUS_OK;
}

/**
 * @brief 读取并清除 IWDG 复位标志。
 * @retval true 本次复位包含 IWDG 标志，且已清除 RCC 复位标志锁存。
 * @retval false 未观察到 IWDG 复位标志，不修改复位标志锁存。
 * @pre 应在启动早期、其他代码清除 RCC 复位标志之前调用。
 * @post 返回 true 时 IWDG 及同组复位标志已被清除；返回 false 时寄存器保持不变。
 * @note 读取结果反映本次上电/复位周期；重复调用通常返回 false。
 */
bool Sys_Watchdog_WasReset(void) {
  return Platform_IWDG_WasReset(PLATFORM_IWDG_1);
}
