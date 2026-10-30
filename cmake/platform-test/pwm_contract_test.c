/**
 * @file pwm_contract_test.c
 * @brief PWM Platform 公共契约测试。
 * @ingroup tests
 */

#include "platform_pwm_backend.h"
#include "platform_pwm.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

static uint32_t s_compare;
static platform_pwm_instance_t s_last_pwm;

/**
 * @brief 判断 PWM 实例是否映射到本系列已启用的 HAL 定时器。
 * @param pwm PWM 定时器身份；测试替身接受 TIM1、TIM2、TIM3、TIM4 四个枚举值。
 * @retval true 参数等于四个受测 PWM 实例之一。
 * @retval false 参数为其他值。
 * @post 只比较枚举值，不读取 HAL 句柄，也不修改测试观察状态。
 * @note 本函数是测试替身；生产后端还会检查对应 HAL 定时器是否存在。
 */
bool platform_pwm_backend_is_valid(platform_pwm_instance_t pwm) {
  return pwm == PLATFORM_PWM_TIM1 ||
         pwm == PLATFORM_PWM_TIM2 ||
         pwm == PLATFORM_PWM_TIM3 ||
         pwm == PLATFORM_PWM_TIM4;
}

/**
 * @brief 启动指定 PWM 定时器的一个通道。
 * @param pwm PWM 定时器身份；测试替身记录到 s_last_pwm，不校验实例映射。
 * @param channel PWM 通道，公共层限定为 1..4；测试替身忽略该参数。
 * @retval PLATFORM_STATUS_OK 测试替身固定返回成功，表示已接受启动请求。
 * @pre 公共 PWM 层已校验实例和通道；生产目标已完成 MX_TIMx_Init()。
 * @post 仅把 pwm 写入 s_last_pwm，不使能比较输出、不启动定时器，也不访问 HAL。
 * @note 本函数是测试替身；生产后端的 BUSY、ERROR 和 INVALID_ARGUMENT 状态不在此模拟。
 */
platform_status_t platform_pwm_backend_start(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  (void)channel;
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 启动指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份；测试替身记录到 s_last_pwm，不校验实例映射。
 * @retval PLATFORM_STATUS_OK 测试替身固定返回成功，表示已接受全部通道启动请求。
 * @pre 公共 PWM 层已校验实例；生产目标已完成 MX_TIMx_Init() 并配置四个 PWM 通道。
 * @post 仅把 pwm 写入 s_last_pwm，不使能比较输出、不启动定时器，也不访问 HAL。
 * @note 本函数是测试替身；生产后端的 BUSY、ERROR 和 INVALID_ARGUMENT 状态不在此模拟。
 */
platform_status_t platform_pwm_backend_start_all(
  platform_pwm_instance_t pwm
) {
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 停止指定 PWM 定时器的一个通道。
 * @param pwm PWM 定时器身份；测试替身记录到 s_last_pwm，不校验实例映射。
 * @param channel PWM 通道，公共层限定为 1..4；测试替身忽略该参数。
 * @retval PLATFORM_STATUS_OK 测试替身固定返回成功，表示已接受停止请求。
 * @pre 公共 PWM 层已校验实例和通道；调用方不再依赖该通道继续输出。
 * @post 仅把 pwm 写入 s_last_pwm，不关闭比较输出、不停止定时器，也不访问 HAL。
 * @note 本函数是测试替身；生产后端的 BUSY、ERROR 和 INVALID_ARGUMENT 状态不在此模拟。
 */
platform_status_t platform_pwm_backend_stop(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel
) {
  (void)channel;
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 停止指定 PWM 定时器的全部四个通道。
 * @param pwm PWM 定时器身份；测试替身记录到 s_last_pwm，不校验实例映射。
 * @retval PLATFORM_STATUS_OK 测试替身固定返回成功，表示已接受全部通道停止请求。
 * @pre 公共 PWM 层已校验实例；生产目标已完成四个通道的 PWM 配置。
 * @post 仅把 pwm 写入 s_last_pwm，不关闭比较输出、不停止定时器，也不访问 HAL。
 * @note 本函数是测试替身；生产后端的 BUSY、ERROR 和 INVALID_ARGUMENT 状态不在此模拟。
 */
platform_status_t platform_pwm_backend_stop_all(
  platform_pwm_instance_t pwm
) {
  s_last_pwm = pwm;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 把计数值写入指定 PWM 通道的 CCR。
 * @param pwm PWM 定时器身份；测试替身记录到 s_last_pwm，不校验实例映射。
 * @param channel PWM 通道，公共层限定为 1..4；测试替身忽略该参数。
 * @param compare 调用方请求的比较计数值；测试替身原样记录到 s_compare，不按 ARR 夹紧。
 * @retval PLATFORM_STATUS_OK 测试替身已记录 pwm 和 compare。
 * @pre 公共 PWM 层已校验实例、通道和比较值；生产目标中的定时器已完成 CubeMX 配置。
 * @post s_last_pwm 等于 pwm，s_compare 等于 compare；不写入 CCR，也不等待更新事件。
 * @note 本函数是测试替身，不访问 HAL；生产后端的 INVALID_ARGUMENT 和寄存器副作用不在此模拟。
 */
platform_status_t platform_pwm_backend_set_compare(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t compare
) {
  (void)channel;
  s_last_pwm = pwm;
  s_compare = compare;
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 按量程把占空比换算为 CCR 并写入目标通道。
 * @param pwm PWM 定时器身份；测试替身记录到 s_last_pwm，不校验实例映射。
 * @param channel PWM 通道，公共层限定为 1..4；测试替身忽略该参数。
 * @param max_duty 占空比量程，无量纲；仅用于零量程判断，不参与换算。
 * @param duty 目标占空比，无量纲；非零量程时测试替身原样记录到 s_compare。
 * @retval PLATFORM_STATUS_OK max_duty 非零，测试替身已记录 pwm 和 duty。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT max_duty 为 0，s_compare 被置为 0。
 * @pre 公共 PWM 层已校验实例、通道和数值范围；生产目标中的定时器已配置有效 ARR。
 * @post s_last_pwm 等于 pwm；max_duty 非零时 s_compare 等于 duty，否则为 0；不写 CCR。
 * @note 本函数是测试替身，不执行 CCR = ARR * duty / max_duty 的硬件换算，也不访问 HAL。
 */
platform_status_t platform_pwm_backend_set_duty(
  platform_pwm_instance_t pwm,
  platform_pwm_channel_t channel,
  uint32_t max_duty,
  uint32_t duty
) {
  (void)channel;
  s_last_pwm = pwm;
  s_compare = max_duty == 0U ? 0U : duty;
  return max_duty == 0U ? PLATFORM_STATUS_INVALID_ARGUMENT : PLATFORM_STATUS_OK;
}

/**
 * @brief 验证 PWM 参数校验与占空比。
 * @pre PWM 公共实现和本文件测试替身已链接；全局 s_compare、s_last_pwm 可写。
 * @post 已覆盖启动、设置比较值、占空比入口、全部停止、非法通道及零量程；
 *       非法通道路径当时置位 TIM1 通用非法参数位，后续成功 TIM1 操作会清除该快照；
 *       终结时 s_last_pwm 为 PLATFORM_PWM_TIM2。
 * @warning 使用 assert 校验每一步；任一断言失败会调用 abort，进程不返回。
 * @note 测试替身不驱动真实定时器，也不执行 CCR 换算；比较值仅保存在 s_compare 中。
 */
static void test_pwm_contract(void) {
  s_compare = 0U;
  s_last_pwm = (platform_pwm_instance_t)0xFFU;
  assert(Platform_PWM_Start(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
  assert(Platform_PWM_SetCompare(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1,
           123U
         ) == PLATFORM_STATUS_OK);
  assert(s_compare == 123U);

  assert(Platform_PWM_Start(
           PLATFORM_PWM_TIM2,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
  assert(s_last_pwm == PLATFORM_PWM_TIM2);
  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM4,
           PLATFORM_PWM_CHANNEL_3,
           20000U,
           1500U
         ) == PLATFORM_STATUS_OK);
  assert(s_last_pwm == PLATFORM_PWM_TIM4);
  assert(s_compare == 1500U);
  assert(Platform_PWM_StopAll(PLATFORM_PWM_TIM3) == PLATFORM_STATUS_OK);
  assert(s_last_pwm == PLATFORM_PWM_TIM3);

  assert(Platform_PWM_SetCompare(
           PLATFORM_PWM_TIM1,
           (platform_pwm_channel_t)99U,
           1U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert((Platform_PWM_GetLastError(PLATFORM_PWM_TIM1) &
          PLATFORM_ERROR_INVALID_ARGUMENT) != 0U);

  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_2,
           1000U,
           500U
         ) == PLATFORM_STATUS_OK);
  assert(s_compare == 500U);
  assert(Platform_PWM_SetDuty(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_2,
           0U,
           0U
         ) == PLATFORM_STATUS_INVALID_ARGUMENT);
  assert(Platform_PWM_Stop(
           PLATFORM_PWM_TIM1,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
  assert(Platform_PWM_Stop(
           PLATFORM_PWM_TIM2,
           PLATFORM_PWM_CHANNEL_1
         ) == PLATFORM_STATUS_OK);
}

/**
 * @brief 运行本文件定义的契约测试并返回进程状态。
 * @retval 0 test_pwm_contract 的全部断言通过。
 * @pre PWM 测试替身已链接，且全局观察变量可写。
 * @post 不创建线程、不分配动态内存，也不启动真实 PWM 外设。
 * @warning 断言失败会直接终止进程，不会返回非零退出码。
 */
int main(void) {
  test_pwm_contract();
  return 0;
}
