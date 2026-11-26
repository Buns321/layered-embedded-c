/**
 * @file rtos_stub.h
 * @brief 宿主机单元测试使用的 CMSIS-RTOS2 与 FreeRTOS 可控桩声明。
 * @ingroup tests
 */

#ifndef LAYERED_CAR_RTOS_STUB_H
#define LAYERED_CAR_RTOS_STUB_H

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum os_kernel_state_e {
  osKernelInactive = 0,
  osKernelReady = 1,
  osKernelRunning = 2,
  osKernelLocked = 3,
  osKernelSuspended = 4,
  osKernelError = -1,
} osKernelState_t;

typedef enum os_priority_e {
  osPriorityNone = 0,
  osPriorityIdle = 1,
  osPriorityLow = 8,
  osPriorityBelowNormal = 16,
  osPriorityNormal = 24,
  osPriorityAboveNormal = 32,
  osPriorityHigh = 40,
  osPriorityRealtime = 48,
} osPriority_t;

typedef int32_t osStatus_t;
typedef void *osThreadId_t;
typedef void *TaskHandle_t;

typedef void (*osThreadFunc_t)(void *argument);
typedef osStatus_t (*rtos_stub_delay_until_fn_t)(uint32_t ticks);

typedef struct static_task_s {
  uint32_t reserved[8];
} StaticTask_t;

typedef struct os_thread_attr_s {
  const char *name;
  uint32_t attr_bits;
  void *cb_mem;
  uint32_t cb_size;
  void *stack_mem;
  uint32_t stack_size;
  osPriority_t priority;
} osThreadAttr_t;

#define osOK 0
#define osErrorParameter (-4)
#define configASSERT(x) assert(x)
#define taskSCHEDULER_NOT_STARTED 0U
#define taskSCHEDULER_RUNNING 1U

typedef uint32_t UBaseType_t;

osKernelState_t osKernelGetState(void);
osStatus_t osDelay(uint32_t ticks);
osStatus_t osDelayUntil(uint32_t ticks);
uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickFreq(void);
osThreadId_t osThreadNew(osThreadFunc_t func, void *argument, const osThreadAttr_t *attr);
osThreadId_t osThreadGetId(void);
uint32_t osThreadGetStackSpace(osThreadId_t thread_id);
uint32_t xTaskGetSchedulerState(void);

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);

void RTOS_Stub_Reset(void);
void RTOS_Stub_SetTickCount(uint32_t tick);
void RTOS_Stub_SetTickFreq(uint32_t freq_hz);
void RTOS_Stub_SetStackSpace(uint32_t free_bytes);
void RTOS_Stub_SetThreadNewFail(bool fail);
void RTOS_Stub_SetDelayUntilStatus(osStatus_t status);
void RTOS_Stub_SetDelayUntilHandler(rtos_stub_delay_until_fn_t fn);
void RTOS_Stub_SetMaxLoopIterations(uint32_t max_iters);
bool RTOS_Stub_ShouldContinueLoop(void);
osThreadFunc_t RTOS_Stub_GetLastThreadFunc(void);
void *RTOS_Stub_GetLastThreadArg(void);
const osThreadAttr_t *RTOS_Stub_GetLastThreadAttr(void);
uint32_t RTOS_Stub_GetLastDelayUntilTick(void);
uint32_t RTOS_Stub_GetStackSpaceCalls(void);

#define taskENTER_CRITICAL() ((void)0)
#define taskEXIT_CRITICAL() ((void)0)
#define taskENTER_CRITICAL_FROM_ISR() (0U)
#define taskEXIT_CRITICAL_FROM_ISR(s) ((void)(s))

#endif /* LAYERED_CAR_RTOS_STUB_H */
