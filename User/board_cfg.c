/**
 * @file board_cfg.c
 * @brief F407 四轮全向小车硬件映射、服务启动与静态装配。
 * @details 依据 doc/hardware-map.md 与 f407vet6.ioc 定义四轮 TB6612、
 *          编码器、WHT101 IMU、JoyCon 遥控与 JustFloat 遥测的静态配置。
 */

#include "board_cfg.h"
#include "bsp_dwt.h"
#include "gpio.h"
#include "sys_angle.h"
#include "sys_log.h"
#if defined(SYS_LOG_USE_RTT)
#include "sys_log_rtt.h"
#endif
#include "tim.h"
#include "usart.h"
#include <stddef.h>

/**
 * @brief 四轮速度环共用的默认 PID 参数配置宏。
 * @note 输出量程 max_out 须与 TIM1 PWM max_duty（1000）匹配；
 *       max_target（1.0 m/s）用于单轮闭环目标安全限幅。
 */
#define BOARD_WHEEL_PID_CFG                                                                        \
  {                                                                                                \
      .kp = 1000.0f,                                                                               \
      .ki = 2000.0f,                                                                               \
      .kd = 5.0f,                                                                                  \
      .max_target = 1.0f,                                                                          \
      .max_out = 1000.0f,                                                                          \
      .max_integral = 400.0f,                                                                      \
      .derivative_tau_s = 0.02f,                                                                   \
  }

/** @brief 整车门面静态实例（零动态分配）。 */
static car_omni_t s_car;
#if defined(SYS_LOG_USE_RTT)
/** @brief SEGGER RTT 日志后端静态实例。 */
static sys_log_rtt_t s_log_rtt;
#endif

/**
 * @brief STM32F407VET6 四轮全向小车静态硬件与控制参数表。
 * @note 轮位俯视按顺时针排列，车头方向为 0 rad，逆时针为正：
 *       wheels[0]=A(前右 315°), wheels[1]=B(后右 225°),
 *       wheels[2]=C(后左 135°), wheels[3]=D(前左 45°)。
 */
static const car_omni_cfg_t s_car_cfg = {
    .control_period_s = 0.01f,          // 必须与 rtos_tasks.c 中 s_control_task 周期 10 ms 严格一致
    .wheel_distance_m = 0.12f,          // 轮心到车体几何中心距离（m）
    .max_linear_mps = 0.30f,            // 车体平动合成线速度上限（m/s）
    .max_angular_radps = 1.50f,         // 车体自转角速度上限（rad/s）
    .drive_frame = CHASSIS_FRAME_WORLD, // 默认启用基于 WHT101 航向角的世界系无头模式
    .wheels =
        {
            // A 轮（前右 315°）：TIM1_CH1(PE9), TIM2(PA5/PB3), 方向 PD0/PD1, 共用 STBY=PC13
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_1, .max_duty = 1000U},
                .encoder = {.htim = &htim2},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_0},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_1},
                .stby = {.port = GPIOC, .pin = GPIO_PIN_13},
                .theta_rad = SYS_PI * 1.75f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
            // B 轮（后右 225°）：TIM1_CH2(PE11), TIM3(PA6/PA7), 方向 PD2/PD3, 共用 STBY=PC13
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_2, .max_duty = 1000U},
                .encoder = {.htim = &htim3},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_2},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_3},
                .stby = {.port = GPIOC, .pin = GPIO_PIN_13},
                .theta_rad = SYS_PI * 1.25f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
            // C 轮（后左 135°）：TIM1_CH3(PE13), TIM4(PD12/PD13), 方向 PD4/PD5, 共用 STBY=PE5
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_3, .max_duty = 1000U},
                .encoder = {.htim = &htim4},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_4},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_5},
                .stby = {.port = GPIOE, .pin = GPIO_PIN_5},
                .theta_rad = SYS_PI * 0.75f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
            // D 轮（前左 45°）：TIM1_CH4(PE14), TIM5(PA0/PA1), 方向 PD6/PD7, 共用 STBY=PE5
            {
                .pwm = {.htim = &htim1, .channel = TIM_CHANNEL_4, .max_duty = 1000U},
                .encoder = {.htim = &htim5},
                .in1 = {.port = GPIOD, .pin = GPIO_PIN_6},
                .in2 = {.port = GPIOD, .pin = GPIO_PIN_7},
                .stby = {.port = GPIOE, .pin = GPIO_PIN_5},
                .theta_rad = SYS_PI * 0.25f,
                .sign = 1.0f,
                .counts_per_meter = 1000.0f,
                .pid = BOARD_WHEEL_PID_CFG,
            },
        },
    // WHT101 IMU：USART1（9600 bps，中断单字节接收），上电自动归零航向角
    .imu_wht101 = {.huart = &huart1, .timeout_ms = 250U, .auto_zero_on_boot = true},
    // JoyCon 蓝牙遥控：USART3（115200 bps，中断单字节接收），3% 摇杆死区防零漂
    .rc_joycon = {.huart = &huart3, .timeout_ms = 300U, .deadband_norm = 0.03f},
    // 遥测输出：USART2 DMA（115200 bps），每 20 ms（50 Hz）发送 16 通道 JustFloat 帧
    .telemetry = {.huart = &huart2, .period_ms = 20U, .use_firewater = false},
    // 控制任务心跳看门狗超时阈值（ms），超时将触发不可逆急停保护
    .control_heartbeat_timeout_ms = 100U,
};

bool Board_BootServices(void) {
  // 初始化 Cortex-M4 DWT 周期计数器，供高精度微秒延时与任务耗时统计使用
  BSP_DWT_Init();
#if defined(SYS_LOG_USE_RTT)
  // 注册并启用 SEGGER RTT 通道 0 作为系统日志后端
  Sys_Log_RTT_Init(&s_log_rtt, "rtt", 0U);
  Sys_Log_Init(&s_log_rtt.base);
  Sys_Log_SetLevel(SYS_LOG_LEVEL_INFO);
  SYS_LOG_INFO("BOOT", "RTT log initialized");
#endif
  return true;
}

const car_omni_cfg_t *Board_GetCarConfig(void) { return &s_car_cfg; }

car_omni_t *Board_InitCar(void) {
  bsp_status_t st = Car_Omni_Init(&s_car, &s_car_cfg);
  if (st != BSP_OK) {
    SYS_LOG_ERROR("BOARD", "car init failed: %u", (unsigned int)st);
    return NULL;
  }
  return &s_car;
}
