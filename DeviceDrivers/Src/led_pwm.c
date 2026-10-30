//
// 由 Buns_ 创建于 2026/10/20。
//

/**
 * @file led_pwm.c
 * @brief PWM LED 驱动实现。
 * @ingroup drivers_concrete
 */

#include "led_pwm.h"
#include <assert.h>

#define LED_PWM_BRIGHTNESS_MAX UINT16_MAX

/**
 * @brief 将 16 位亮度映射为 PWM 占空比并写入通道。
 * @param led_pwm_h 已初始化的 PWM LED 对象；非 NULL，且 pwm_instance/pwm_channel 必须有效。
 * @param brightness 亮度，范围 0..UINT16_MAX；0 对应 0%，UINT16_MAX 对应 100%。
 * @pre led_pwm_h 已由 Dev_LED_PWM_Init 绑定到已启动且 ARR 大于 0 的 PWM 通道。
 * @pre 仅在本编译单元或 led_ops_pwm 回调中使用；同一通道不得同时由其他写者更新。
 * @post 已尝试把 brightness 写入占空比，映射由 Platform_PWM_SetDuty 按 0..UINT16_MAX 量程完成。
 * @note 返回的 platform_status_t 被有意丢弃，因此失败不会传回本函数；失败原因记录在 Platform_PWM_GetLastError。
 * @warning 不持有锁，也不重试；与 ISR 或其他任务并发写同一通道时，最终占空比取决于调用顺序。
 */
static void dev_led_pwm_writebrightness(led_pwm_t *led_pwm_h, uint16_t brightness) {
  assert(led_pwm_h);
  (void)Platform_PWM_SetDuty(
    led_pwm_h->pwm_instance,
    led_pwm_h->pwm_channel,
    LED_PWM_BRIGHTNESS_MAX,
    brightness
  );
}

/**
 * @brief 将 PWM LED 占空比设为 100%。
 * @param led_h PWM LED 的 led_base 视图；非 NULL，且必须来自已初始化的 led_pwm_t 首位成员。
 * @pre led_h 已由 Dev_LED_PWM_Init 初始化，PWM 通道已启动。
 * @pre 仅供 Dev_LED_Base_TurnOn 通过 led_ops_pwm 调用。
 * @post 通道占空比被写为 UINT16_MAX 量程，对应 100%；led_base.is_on 由基类在回调返回后置为 DEV_LED_ON。
 * @note 对象不保存上次亮度，因此“打开”不会恢复先前调光值，而是固定全亮。
 * @warning 调光失败被静默忽略；若通道未启动或实例非法，硬件亮度不变而基类软件状态仍可能显示为开启。
 */
static void dev_led_pwm_turnon(led_base_t *led_h) {
  assert(led_h);
  dev_led_pwm_writebrightness((led_pwm_t *)led_h, LED_PWM_BRIGHTNESS_MAX);
}

/**
 * @brief 将 PWM LED 占空比置零以关闭输出。
 * @param led_h PWM LED 的 led_base 视图；非 NULL，且必须来自已初始化的 led_pwm_t 首位成员。
 * @pre led_h 已由 Dev_LED_PWM_Init 初始化，PWM 通道已启动。
 * @pre 仅供 Dev_LED_Base_TurnOff 通过 led_ops_pwm 调用。
 * @post 通道占空比被写为 0，对应 0%；led_base.is_on 由基类在回调返回后置为 DEV_LED_OFF。
 * @note 该实现不停止 PWM 定时器，只改变比较值；对象不保存额外亮度。
 * @warning 写入失败被静默忽略，硬件可能保持原亮度而基类软件状态已变为关闭。
 */
static void dev_led_pwm_turnoff(led_base_t *led_h) {
  assert(led_h);
  dev_led_pwm_writebrightness((led_pwm_t *)led_h, 0u);
}

/**
 * @brief 将 PWM LED 更新到指定亮度。
 * @param led_h PWM LED 的 led_base 视图；非 NULL，且必须来自已初始化的 led_pwm_t 首位成员。
 * @param brightness 亮度，范围 0..UINT16_MAX；0 表示关闭，UINT16_MAX 表示全亮。
 * @pre led_h 已由 Dev_LED_PWM_Init 初始化，PWM 通道已启动。
 * @pre 仅供 Dev_LED_Base_SetBrightness 通过 led_ops_pwm 调用；对象字段不缓存 brightness。
 * @post 已尝试按 brightness 更新占空比；led_base.is_on 由基类在回调返回后按 brightness 是否为零更新。
 * @note 不支持时写入错误只进入 Platform PWM 错误快照，不改变本函数签名。
 * @warning 非 ISR 安全；同一通道的并发写入会相互覆盖，且软件 is_on 与硬件状态可能不一致。
 */
static void dev_led_pwm_setbrightness(led_base_t *led_h, uint16_t brightness) {
  assert(led_h);
  dev_led_pwm_writebrightness((led_pwm_t *)led_h, brightness);
}

static const led_ops_t led_ops_pwm = {
  .on             = dev_led_pwm_turnon,
  .off            = dev_led_pwm_turnoff,
  .set_brightness = dev_led_pwm_setbrightness,
};

/**
 * @brief 初始化 PWM LED 并启动指定通道输出。
 * @param led_h 待初始化对象；非 NULL，生命周期和通道独占访问责任由调用方承担。
 * @param name 设备名称；只读字符串，由 led_base.name 保存指针而不复制。
 * @param pwm_instance PWM 定时器身份；必须是当前构建启用的实例并已完成 MX_TIMx_Init()。
 * @param pwm_channel PWM 通道身份；必须为 PLATFORM_PWM_CHANNEL_1..4，且未由其他对象独占。
 * @retval PLATFORM_STATUS_OK PWM 通道已启动，对象可用于开关和调光。
 * @retval PLATFORM_STATUS_ERROR 定时器启动失败；对象已绑定但输出不可用。
 * @retval PLATFORM_STATUS_BUSY 定时器或通道正忙；对象已绑定但输出不可用。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例或通道非法；对象已绑定但启动请求被拒绝。
 * @pre 对应定时器已在 CubeMX/Core 中配置为 PWM 模式，ARR 大于 0，且调用发生在控制或显示任务启动前。
 * @pre led_h 和 name 非 NULL；本函数只对这两个空指针使用调试断言。
 * @post led_base 已绑定 PWM 操作表并记录 DEV_LED_OFF，pwm_instance/pwm_channel 已保存，且启动前已尝试写入零占空比。
 * @note 不分配内存，不复制 name；零占空比写入发生在 PWM 启动前，其失败状态被丢弃，可随后查询 Platform_PWM_GetLastError。
 * @warning 非 ISR 安全；同一 PWM 定时器或通道不得被多个 LED/驱动无协调地启动或改占空比。
 */
platform_status_t Dev_LED_PWM_Init(
  led_pwm_t             *led_h,
  const char            *name,
  platform_pwm_instance_t  pwm_instance,
  platform_pwm_channel_t   pwm_channel
) {
  assert(led_h && name);

  Dev_LED_Base_Init(&led_h->led_base, name, &led_ops_pwm);
  led_h->pwm_instance = pwm_instance;
  led_h->pwm_channel  = pwm_channel;

  dev_led_pwm_writebrightness(led_h, 0u);

  return Platform_PWM_Start(pwm_instance, pwm_channel);
}
