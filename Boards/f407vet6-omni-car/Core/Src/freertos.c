/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for MonitorTask */
osThreadId_t MonitorTaskHandle;
uint32_t MonitorTaskStack[ 512 ];
osStaticThreadDef_t MonitorTaskTCB;
const osThreadAttr_t MonitorTask_attributes = {
  .name = "MonitorTask",
  .cb_mem = &MonitorTaskTCB,
  .cb_size = sizeof(MonitorTaskTCB),
  .stack_mem = &MonitorTaskStack[0],
  .stack_size = sizeof(MonitorTaskStack),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for ControlTask */
osThreadId_t ControlTaskHandle;
uint32_t ControlTaskStack[ 1024 ];
osStaticThreadDef_t ControlTaskTCB;
const osThreadAttr_t ControlTask_attributes = {
  .name = "ControlTask",
  .cb_mem = &ControlTaskTCB,
  .cb_size = sizeof(ControlTaskTCB),
  .stack_mem = &ControlTaskStack[0],
  .stack_size = sizeof(ControlTaskStack),
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for UARTTask */
osThreadId_t UARTTaskHandle;
uint32_t UARTTaskStack[ 1024 ];
osStaticThreadDef_t UARTTaskTCB;
const osThreadAttr_t UARTTask_attributes = {
  .name = "UARTTask",
  .cb_mem = &UARTTaskTCB,
  .cb_size = sizeof(UARTTaskTCB),
  .stack_mem = &UARTTaskStack[0],
  .stack_size = sizeof(UARTTaskStack),
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void RTOS_Monitor_StartTask(void *argument);
void RTOS_Control_StartTask(void *argument);
void RTOS_UART_StartTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of MonitorTask */
  MonitorTaskHandle = osThreadNew(RTOS_Monitor_StartTask, NULL, &MonitorTask_attributes);

  /* creation of ControlTask */
  ControlTaskHandle = osThreadNew(RTOS_Control_StartTask, NULL, &ControlTask_attributes);

  /* creation of UARTTask */
  UARTTaskHandle = osThreadNew(RTOS_UART_StartTask, NULL, &UARTTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  if (
    MonitorTaskHandle == NULL ||
    ControlTaskHandle == NULL ||
    UARTTaskHandle == NULL
  ) {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_RTOS_Monitor_StartTask */
/**
* @brief 监控任务弱默认实现。
* @see RTOS_Monitor_StartTask
*/
/* USER CODE END Header_RTOS_Monitor_StartTask */
__weak void RTOS_Monitor_StartTask(void *argument)
{
  /* USER CODE BEGIN RTOS_Monitor_StartTask */
  (void)argument;
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END RTOS_Monitor_StartTask */
}

/* USER CODE BEGIN Header_RTOS_Control_StartTask */
/**
* @brief 控制任务弱默认实现。
* @see RTOS_Control_StartTask
*/
/* USER CODE END Header_RTOS_Control_StartTask */
__weak void RTOS_Control_StartTask(void *argument)
{
  /* USER CODE BEGIN RTOS_Control_StartTask */
  (void)argument;
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END RTOS_Control_StartTask */
}

/* USER CODE BEGIN Header_RTOS_UART_StartTask */
/**
* @brief UART 任务弱默认实现。
* @see RTOS_UART_StartTask
*/
/* USER CODE END Header_RTOS_UART_StartTask */
__weak void RTOS_UART_StartTask(void *argument)
{
  /* USER CODE BEGIN RTOS_UART_StartTask */
  (void)argument;
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END RTOS_UART_StartTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

