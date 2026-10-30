/**
 * @file platform_iwdg.c
 * @brief STM32F1 公共 IWDG 实现的 HAL 后端。
 * @ingroup platform
 */

#include "platform_hal_internal.h"
#include "platform_iwdg_backend.h"
#include "stm32f1xx_hal.h"

#define PLATFORM_IWDG_LSI_HZ 40000u

static IWDG_HandleTypeDef s_iwdg_h;

/**
 * @brief 把 IWDG 预分频除数转换为 HAL 预分频宏。
 * @param divisor 无量纲预分频除数，只接受 4、8、16、32、64、128 或 256。
 * @param prescaler 输出参数，成功时写入 HAL 宏；不得为 NULL。
 * @retval true 映射成功且已写入 prescaler。
 * @retval false divisor 不受支持或 prescaler 为 NULL；不写入 prescaler。
 * @pre prescaler 指向可写 uint32_t；调用方串行化同一输出对象。
 * @note 纯映射，不访问硬件、不分配内存，可在任务或中断上下文调用。
 */
static bool platform_iwdg_hal_prescaler(
  uint32_t divisor,
  uint32_t *prescaler
) {
  if (prescaler == NULL) {
    return false;
  }

  switch (divisor) {
    case 4u:   *prescaler = IWDG_PRESCALER_4; return true;
    case 8u:   *prescaler = IWDG_PRESCALER_8; return true;
    case 16u:  *prescaler = IWDG_PRESCALER_16; return true;
    case 32u:  *prescaler = IWDG_PRESCALER_32; return true;
    case 64u:  *prescaler = IWDG_PRESCALER_64; return true;
    case 128u: *prescaler = IWDG_PRESCALER_128; return true;
    case 256u: *prescaler = IWDG_PRESCALER_256; return true;
    default:   return false;
  }
}

/**
 * @brief 返回后端用于 IWDG 时基计算的 LSI 标称频率。
 * @return LSI 标称频率，单位 Hz；F1 为 40000，F4 为 32000。
 * @note 该值是编译期标称值，不读取芯片校准数据；实际 LSI 偏差会反映到看门狗超时。
 * @note 纯读取，不访问硬件、不分配内存，可在任务或中断上下文调用。
 */
uint32_t platform_iwdg_backend_lsi_hz(void) {
  return PLATFORM_IWDG_LSI_HZ;
}

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
) {
  uint32_t prescaler;
  if (!platform_iwdg_hal_prescaler(divisor, &prescaler)) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  s_iwdg_h.Instance = IWDG;
  s_iwdg_h.Init.Prescaler = prescaler;
  s_iwdg_h.Init.Reload = reload;
  return platform_from_hal_status(HAL_IWDG_Init(&s_iwdg_h));
}

/**
 * @brief 刷新已启动的 IWDG 计数器。
 * @retval PLATFORM_STATUS_OK HAL 已接受重装命令。
 * @pre 已成功调用 platform_iwdg_backend_start()；本函数不检查启动状态。
 * @post 成功时计数器从重装值继续递减。
 * @warning 必须在超时前调用；过晚调用会导致芯片复位。
 * @note 当前 HAL 刷新路径恒返回成功；不分配内存、无锁，调用方必须串行化并发刷新。
 */
platform_status_t platform_iwdg_backend_refresh(void) {
  return platform_from_hal_status(HAL_IWDG_Refresh(&s_iwdg_h));
}

/**
 * @brief 读取并清除本次 IWDG 复位标志。
 * @retval true RCC 复位状态包含 IWDG 标志，并已清除复位标志锁存。
 * @retval false 未观察到 IWDG 复位标志，不修改寄存器。
 * @post true 分支写入 RMVF，清除全部共享的 STM32 复位标志。
 * @warning 清除动作影响所有复位源，其他模块随后可能无法再读取本次复位原因。
 * @note 直接访问 RCC->CSR；不分配内存，读取与清除不构成原子操作，需由调用方串行化。
 */
bool platform_iwdg_backend_was_reset(void) {
  const bool was_reset = (RCC->CSR & RCC_CSR_IWDGRSTF) != 0U;
  if (was_reset) {
    RCC->CSR |= RCC_CSR_RMVF;
  }
  return was_reset;
}
