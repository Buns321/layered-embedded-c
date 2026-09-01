//
// Created by Buns_ on 2026/8/31.
//

#include "../Inc/rtos_usart.h"

/*
 * TODO: 在这里自己实现 BT 任务逻辑（收字节 → 喂驱动 → 读指令 → 塞队列）。
 *
 * 可用的驱动 API 见 DeviceDrivers/Inc/usart_controler.h：
 *   Dev_USART_Controler_Init    (&s_bt, PLATFORM_USART_1);   // 初始化
 *   Dev_USART_Controler_FeedByte(&s_bt, byte);               // 逐字节喂（串口收一个喂一个）
 *   bt_cmd_t cmd = Dev_USART_Controler_GetCmd(&s_bt);        // 读最新运动指令(vx/vy/omega)
 *
 * 共享队列句柄见 rtos_shared.h：cmd_queue（元素类型 bt_cmd_t）。
 */
