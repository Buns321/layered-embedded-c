/**
 * @file watchdog_contract_test.c
 * @brief IWDG/WWDG Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_iwdg_backend.h"
#include "platform_wwdg_backend.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

static platform_status_t s_iwdg_start_result;
static platform_status_t s_wwdg_start_result;
static uint32_t s_iwdg_lsi_hz;
static uint32_t s_wwdg_pclk1_hz;
static bool s_iwdg_backend_started;
static bool s_wwdg_backend_started;
static uint32_t s_iwdg_divisor;
static uint32_t s_iwdg_reload;
static uint32_t s_wwdg_divisor;
static uint8_t s_wwdg_window;

/**
 * @brief 返回测试注入的 IWDG LSI 标称频率。
 * @return s_iwdg_lsi_hz 当前值，单位 Hz；测试可设为 32000 或其他正频率。
 * @post 不修改测试状态，不读取芯片校准数据，也不访问 RCC。
 * @note 本函数是测试替身；生产后端返回系列的编译期 LSI 标称频率。
 */
uint32_t platform_iwdg_backend_lsi_hz(void) {
  return s_iwdg_lsi_hz;
}

/**
 * @brief 配置并启动 IWDG 后端。
 * @param divisor 预分频除数，无量纲；公共层限定为 4、8、16、32、64、128 或 256，测试替身原样记录。
 * @param reload 12 位重装计数值，范围 0..0x0FFF；测试替身原样记录，不校验或夹紧。
 * @return 注入值为 PLATFORM_STATUS_OK 时返回 OK 并置 s_iwdg_backend_started；否则原样返回
 *         s_iwdg_start_result，且不置启动标志。
 * @pre 公共 IWDG 层已完成实例、超时和配置校验；测试需先设置 s_iwdg_start_result。
 * @post 无论结果如何都会写入 s_iwdg_divisor 和 s_iwdg_reload；仅 OK 路径置位 s_iwdg_backend_started。
 * @warning 测试替身不校验 divisor 或 reload，也不通过 assert 拒绝非法参数；它只记录参数并返回注入状态。
 *          本替身不启动真实看门狗，不模拟不可逆硬件副作用。
 * @note 本函数是测试替身，不访问 HAL；生产后端的 INVALID_ARGUMENT、TIMEOUT 和不可回滚启动语义仍由 Platform 契约定义。
 */
platform_status_t platform_iwdg_backend_start(
  uint32_t divisor,
  uint32_t reload
) {
  s_iwdg_divisor = divisor;
  s_iwdg_reload = reload;
  if (s_iwdg_start_result != PLATFORM_STATUS_OK) {
    return s_iwdg_start_result;
  }
  s_iwdg_backend_started = true;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 刷新已启动的 IWDG 计数器。
 * @retval PLATFORM_STATUS_OK s_iwdg_backend_started 为 true，测试替身表示已接受重装命令。
 * @retval PLATFORM_STATUS_ERROR s_iwdg_backend_started 为 false，表示尚未成功启动。
 * @pre 生产调用方应已成功启动 IWDG；测试替身通过 s_iwdg_backend_started 检查该前置状态。
 * @post 不修改 s_iwdg_backend_started，不访问 HAL，也不改变计数器。
 * @warning 测试替身不会触发真实看门狗复位；生产路径若过晚刷新仍会导致芯片复位。
 * @note 本函数是测试替身，仅用启动标志产生 OK/ERROR 两种状态。
 */
platform_status_t platform_iwdg_backend_refresh(void) {
  return s_iwdg_backend_started ? PLATFORM_STATUS_OK : PLATFORM_STATUS_ERROR;
}

/**
 * @brief 测试替身固定报告观察到 IWDG 复位。
 * @retval true 测试替身始终返回 true，用于覆盖公共层的实例门控和状态传播。
 * @pre 无；该测试替身不要求 IWDG 已启动，也不读取启动标志。
 * @post 不修改测试状态、RCC 复位标志或任何硬件寄存器。
 * @warning 该结果不代表真实复位原因；生产后端会读取并清除共享 RCC 复位锁存。
 * @note 本函数是测试替身，不访问 RCC->CSR。
 */
bool platform_iwdg_backend_was_reset(void) {
  return true;
}

/**
 * @brief 返回测试注入的 APB1 时钟频率。
 * @return s_wwdg_pclk1_hz 当前值，单位 Hz；测试可设为 42 MHz、72 MHz 或其他正频率。
 * @post 不修改测试状态，不读取 RCC 时钟配置，也不访问 WWDG。
 * @note 本函数是测试替身；生产后端返回 HAL 当前报告的 PCLK1 频率。
 */
uint32_t platform_wwdg_backend_pclk1_hz(void) {
  return s_wwdg_pclk1_hz;
}

/**
 * @brief 记录 WWDG 配置并返回注入的启动状态。
 * @param divisor WWDG 预分频除数；公共层限定为 1、2、4、8，测试替身原样记录。
 * @param window 原始刷新窗口值；公共层限定为 PLATFORM_WWDG_WINDOW_MIN..MAX，测试替身原样记录。
 * @return 注入值为 PLATFORM_STATUS_OK 时返回 OK 并置 s_wwdg_backend_started；否则原样返回
 *         s_wwdg_start_result，且不置启动标志。
 * @pre 公共 WWDG 层已完成实例、超时和窗口校验；测试需先设置 s_wwdg_start_result。
 * @post 无论结果如何都会写入 s_wwdg_divisor 和 s_wwdg_window；仅 OK 路径置位 s_wwdg_backend_started。
 * @warning 测试替身不校验参数，也不启动真实看门狗；生产后端的不可逆启动和未刷新复位风险仍由 Platform 契约定义。
 * @note 本函数是测试替身，不访问 HAL、时钟或寄存器。
 */
platform_status_t platform_wwdg_backend_start(
  uint32_t divisor,
  uint8_t window
) {
  s_wwdg_divisor = divisor;
  s_wwdg_window = window;
  if (s_wwdg_start_result != PLATFORM_STATUS_OK) {
    return s_wwdg_start_result;
  }
  s_wwdg_backend_started = true;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 刷新已启动的 WWDG 计数器。
 * @retval PLATFORM_STATUS_OK s_wwdg_backend_started 为 true，测试替身表示已接受刷新命令。
 * @retval PLATFORM_STATUS_ERROR s_wwdg_backend_started 为 false，表示尚未成功启动。
 * @pre 生产调用方应已成功启动 WWDG；测试替身通过 s_wwdg_backend_started 检查该前置状态。
 * @post 不修改 s_wwdg_backend_started，不访问 HAL，也不改变计数器。
 * @warning 测试替身不检查窗口时序，也不会触发真实看门狗复位。
 * @note 本函数是测试替身，仅用启动标志产生 OK/ERROR 两种状态。
 */
platform_status_t platform_wwdg_backend_refresh(void) {
  return s_wwdg_backend_started ? PLATFORM_STATUS_OK : PLATFORM_STATUS_ERROR;
}

/**
 * @brief 测试替身固定报告观察到 WWDG 复位。
 * @retval true 测试替身始终返回 true，用于覆盖公共层的实例门控和状态传播。
 * @pre 无；该测试替身不要求 WWDG 已启动，也不读取启动标志。
 * @post 不修改测试状态、RCC 复位标志或任何硬件寄存器。
 * @warning 该结果不代表真实复位原因；生产后端会读取并清除共享 RCC 复位锁存。
 * @note 本函数是测试替身，不访问 RCC->CSR。
 */
bool platform_wwdg_backend_was_reset(void) {
  return true;
}

/**
 * @brief 验证 IWDG 参数计算与启动。
 * @post 成功路径验证非法实例和零超时被拒绝、注入失败传播、启动后刷新成功、重复启动返回 BUSY，以及复位标志实例门控。
 * @note 后端启动成功后，测试替身保存的 divisor 与 reload 均非零；s_iwdg_backend_started 为 true。
 * @warning 任一 assert 失败都会终止测试进程；静态注入状态和公共层启动状态可能在失败路径部分更新。
 */
static void test_iwdg_contract(void) {
  s_iwdg_lsi_hz = 32000U;
  s_iwdg_start_result = PLATFORM_STATUS_ERROR;

  assert(Platform_IWDG_Start((platform_iwdg_instance_t)99U, 100U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 0U) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 100U) == PLATFORM_STATUS_ERROR);
  assert(Platform_IWDG_Refresh(PLATFORM_IWDG_1) == PLATFORM_STATUS_ERROR);

  s_iwdg_start_result = PLATFORM_STATUS_OK;
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 100U) == PLATFORM_STATUS_OK);
  assert(s_iwdg_divisor != 0U);
  assert(s_iwdg_reload != 0U);
  assert(Platform_IWDG_Refresh(PLATFORM_IWDG_1) == PLATFORM_STATUS_OK);
  assert(Platform_IWDG_Start(PLATFORM_IWDG_1, 100U) == PLATFORM_STATUS_BUSY);
  assert(Platform_IWDG_WasReset(PLATFORM_IWDG_1));
  assert(!Platform_IWDG_WasReset((platform_iwdg_instance_t)99U));
}

/**
 * @brief 验证 WWDG 参数计算与启动。
 * @post 成功路径验证最大超时随时钟变化、空配置和越界窗口被拒绝、注入失败传播、启动后刷新与重复启动 BUSY。
 * @note 最后将 PCLK1 改为 72 MHz，确认最大超时从 49 ms 降为 29 ms。
 * @warning 任一 assert 失败都会终止测试进程；测试替身不启动真实 WWDG，也不验证窗口刷新时序。
 */
static void test_wwdg_contract(void) {
  s_wwdg_pclk1_hz = 42000000U;
  assert(Platform_WWDG_GetMaxTimeoutMs(PLATFORM_WWDG_1) == 49U);
  assert(Platform_WWDG_GetMaxTimeoutMs((platform_wwdg_instance_t)99U) == 0U);

  const platform_wwdg_config_t invalid_window = {
    .timeout_ms = 10U,
    .window = PLATFORM_WWDG_WINDOW_MAX + 1U,
  };
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, NULL) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &invalid_window) ==
         PLATFORM_STATUS_INVALID_ARGUMENT);

  s_wwdg_start_result = PLATFORM_STATUS_ERROR;
  const platform_wwdg_config_t config = {
    .timeout_ms = 10U,
    .window = PLATFORM_WWDG_WINDOW_MIN,
  };
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &config) == PLATFORM_STATUS_ERROR);
  assert(Platform_WWDG_Refresh(PLATFORM_WWDG_1) == PLATFORM_STATUS_ERROR);

  s_wwdg_start_result = PLATFORM_STATUS_OK;
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &config) == PLATFORM_STATUS_OK);
  assert(s_wwdg_divisor != 0U);
  assert(s_wwdg_window == PLATFORM_WWDG_WINDOW_MIN);
  assert(Platform_WWDG_Refresh(PLATFORM_WWDG_1) == PLATFORM_STATUS_OK);
  assert(Platform_WWDG_Start(PLATFORM_WWDG_1, &config) == PLATFORM_STATUS_BUSY);
  assert(Platform_WWDG_WasReset(PLATFORM_WWDG_1));
  assert(!Platform_WWDG_WasReset((platform_wwdg_instance_t)99U));

  s_wwdg_pclk1_hz = 72000000U;
  assert(Platform_WWDG_GetMaxTimeoutMs(PLATFORM_WWDG_1) == 29U);
}

/**
 * @brief 依次运行 IWDG 与 WWDG 公共契约用例。
 * @return 两个用例完成后返回 0。
 * @post 成功路径覆盖参数校验、后端失败传播、启动状态、刷新和复位标志实例检查。
 * @note 任一 assert 失败都会立即终止测试进程，不会返回失败状态。
 * @warning 测试使用看门狗后端桩，不访问硬件寄存器或验证真实超时触发。
 */
int main(void) {
  test_iwdg_contract();
  test_wwdg_contract();
  return 0;
}
