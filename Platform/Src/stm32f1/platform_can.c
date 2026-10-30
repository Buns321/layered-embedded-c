/**
 * @file platform_can.c
 * @brief STM32F1 公共 CAN 实现的 HAL 后端。
 * @ingroup platform
 */

#include "can.h"
#include "platform_can_backend.h"
#include "platform_hal_internal.h"
#include "stm32f1xx_hal.h"
#include <string.h>

#if !PLATFORM_ENABLE_CAN1
#error "STM32F1 Platform CAN requires CAN1"
#endif

/**
 * @brief 把平台 CAN 实例编号转换为 HAL 句柄。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @return 对应的 CAN HAL 句柄；实例未启用或枚举值非法时返回 NULL。
 * @post 只执行实例映射，不启动外设，也不访问 CAN 寄存器。
 * @note 内部辅助函数，可在任务或中断上下文调用；不分配动态内存。
 */
static CAN_HandleTypeDef *platform_can_to_hal(platform_can_instance_t can) {
  return can == PLATFORM_CAN_1 ? &hcan1 : NULL;
}

/**
 * @brief 把 HAL CAN 句柄识别为平台实例编号。
 * @param can_h CAN HAL 句柄指针；允许为 NULL。
 * @param can 输出指针，成功时接收对应平台实例；允许为 NULL，此时不写入。
 * @retval true 句柄属于已启用实例，且 can 非 NULL 时已写入实例编号。
 * @retval false 句柄为 NULL、未知或 can 为 NULL；输出未被写入。
 * @post 只做句柄比较，不修改 CAN 状态或错误快照。
 * @note 内部辅助函数，可在中断上下文调用；不分配动态内存。
 */
static bool platform_hal_to_can(
  CAN_HandleTypeDef *can_h,
  platform_can_instance_t *can
) {
  if (can_h == &hcan1 && can != NULL) {
    *can = PLATFORM_CAN_1;
    return true;
  }
  return false;
}

/**
 * @brief 把 CAN 过滤器标识符转换为硬件位布局。
 * @param config 过滤器配置；调用方拥有对象，本层只读取 id_type 和 remote。
 * @param identifier 已校验的 CAN 标识符；标准帧范围 0..0x7FF，扩展帧范围 0..0x1FFFFFFF。
 * @return 含 IDE、RTR 位并移动到 32 位过滤器寄存器布局的标识符字，不是原始 CAN ID。
 * @pre config 非 NULL，id_type 和 remote 有效，identifier 已按 id_type 校验。
 * @post 只执行位变换，不写过滤器寄存器。
 * @note 内部辅助函数，不校验 identifier 范围；调用方必须先用公共数据校验规则检查。
 */
static uint32_t platform_can_filter_identifier(
  const platform_can_filter_config_t *config,
  uint32_t identifier
) {
  if (config->id_type == PLATFORM_CAN_ID_EXTENDED) {
    return (identifier << 3U) | 0x00000004UL |
           (config->remote ? 0x00000002UL : 0U);
  }
  return (identifier << 21U) |
         (config->remote ? 0x00000002UL : 0U);
}

/**
 * @brief 把 CAN 过滤器掩码转换为硬件位布局。
 * @param config 过滤器配置；调用方拥有对象，本层只读取 mask 和 id_type。
 * @return 含 IDE、RTR 掩码位并移动到 32 位过滤器寄存器布局的掩码字。
 * @pre config 非 NULL，id_type 有效，mask 已按 id_type 校验。
 * @post 只执行位变换，不写过滤器寄存器。
 * @note 内部辅助函数，不校验 mask 范围；调用方必须根据 id_type 预先校验。
 */
static uint32_t platform_can_filter_mask(
  const platform_can_filter_config_t *config
) {
  if (config->id_type == PLATFORM_CAN_ID_EXTENDED) {
    return (config->mask << 3U) | 0x00000006UL;
  }
  return (config->mask << 21U) | 0x00000006UL;
}

/**
 * @brief 把 CAN 通知选择转换为 HAL 中断掩码。
 * @param notifications platform_can_notification_t 位掩码；未知位会被忽略。
 * @return 与已定义通知位对应的 CAN_IT_* HAL 中断掩码；无匹配位时为 0。
 * @post 只执行位映射，不启用或停用任何中断。
 * @note 内部辅助函数，不校验掩码是否为空或是否含未知位；公共层负责校验。
 */
static uint32_t platform_can_notifications(uint32_t notifications) {
  uint32_t hal_notifications = 0U;
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO0_PENDING) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO0_MSG_PENDING;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO1_PENDING) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO1_MSG_PENDING;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO0_FULL) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO0_FULL;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_RX_FIFO1_FULL) != 0U) {
    hal_notifications |= CAN_IT_RX_FIFO1_FULL;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_TX_MAILBOX_EMPTY) != 0U) {
    hal_notifications |= CAN_IT_TX_MAILBOX_EMPTY;
  }
  if ((notifications & PLATFORM_CAN_NOTIFY_ERROR) != 0U) {
    hal_notifications |= CAN_IT_ERROR;
  }
  return hal_notifications;
}

/**
 * @brief 把 HAL CAN 错误位映射为平台错误码。
 * @param hal_error HAL_CAN_GetError() 返回的 HAL 错误位掩码。
 * @return platform_can_error_t 位掩码；无已知错误位时为 NONE，含未定义位时附加 UNKNOWN。
 * @post 只做位映射，不读取或清除 HAL 错误状态，也不改变总线状态。
 * @note 内部辅助函数，可在中断上下文调用；不访问外设寄存器。
 */
static uint32_t platform_can_map_error(uint32_t hal_error) {
  uint32_t error = PLATFORM_CAN_ERROR_NONE;
  if ((hal_error & HAL_CAN_ERROR_EWG) != 0U) {
    error |= PLATFORM_CAN_ERROR_WARNING;
  }
  if ((hal_error & HAL_CAN_ERROR_EPV) != 0U) {
    error |= PLATFORM_CAN_ERROR_PASSIVE;
  }
  if ((hal_error & HAL_CAN_ERROR_BOF) != 0U) {
    error |= PLATFORM_CAN_ERROR_BUS_OFF;
  }
  if ((hal_error & HAL_CAN_ERROR_STF) != 0U) {
    error |= PLATFORM_CAN_ERROR_STUFF;
  }
  if ((hal_error & HAL_CAN_ERROR_FOR) != 0U) {
    error |= PLATFORM_CAN_ERROR_FORM;
  }
  if ((hal_error & HAL_CAN_ERROR_ACK) != 0U) {
    error |= PLATFORM_CAN_ERROR_ACK;
  }
  if ((hal_error & HAL_CAN_ERROR_BR) != 0U) {
    error |= PLATFORM_CAN_ERROR_BIT_RECESSIVE;
  }
  if ((hal_error & HAL_CAN_ERROR_BD) != 0U) {
    error |= PLATFORM_CAN_ERROR_BIT_DOMINANT;
  }
  if ((hal_error & HAL_CAN_ERROR_CRC) != 0U) {
    error |= PLATFORM_CAN_ERROR_CRC;
  }
  if ((hal_error & HAL_CAN_ERROR_RX_FOV0) != 0U) {
    error |= PLATFORM_CAN_ERROR_RX_FIFO0_OVERRUN;
  }
  if ((hal_error & HAL_CAN_ERROR_RX_FOV1) != 0U) {
    error |= PLATFORM_CAN_ERROR_RX_FIFO1_OVERRUN;
  }
  if (((hal_error & HAL_CAN_ERROR_TX_ALST0) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_ALST1) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_ALST2) != 0U)) {
    error |= PLATFORM_CAN_ERROR_TX_ARBITRATION_LOST;
  }
  if (((hal_error & HAL_CAN_ERROR_TX_TERR0) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_TERR1) != 0U) ||
      ((hal_error & HAL_CAN_ERROR_TX_TERR2) != 0U)) {
    error |= PLATFORM_CAN_ERROR_TX_FAILURE;
  }
  if ((hal_error & HAL_CAN_ERROR_TIMEOUT) != 0U) {
    error |= PLATFORM_CAN_ERROR_TIMEOUT;
  }
#ifdef HAL_CAN_ERROR_NOT_INITIALIZED
  if ((hal_error & HAL_CAN_ERROR_NOT_INITIALIZED) != 0U) {
    error |= PLATFORM_CAN_ERROR_STATE;
  }
#endif
#ifdef HAL_CAN_ERROR_NOT_READY
  if ((hal_error & HAL_CAN_ERROR_NOT_READY) != 0U) {
    error |= PLATFORM_CAN_ERROR_STATE;
  }
#endif
#ifdef HAL_CAN_ERROR_NOT_STARTED
  if ((hal_error & HAL_CAN_ERROR_NOT_STARTED) != 0U) {
    error |= PLATFORM_CAN_ERROR_STATE;
  }
#endif
#ifdef HAL_CAN_ERROR_PARAM
  if ((hal_error & HAL_CAN_ERROR_PARAM) != 0U) {
    error |= PLATFORM_CAN_ERROR_PARAMETER;
  }
#endif
#ifdef HAL_CAN_ERROR_INTERNAL
  if ((hal_error & HAL_CAN_ERROR_INTERNAL) != 0U) {
    error |= PLATFORM_CAN_ERROR_INTERNAL;
  }
#endif
#ifdef HAL_CAN_ERROR_INVALID_CALLBACK
  if ((hal_error & HAL_CAN_ERROR_INVALID_CALLBACK) != 0U) {
    error |= PLATFORM_CAN_ERROR_PARAMETER;
  }
#endif

  uint32_t known_errors =
    HAL_CAN_ERROR_EWG |
    HAL_CAN_ERROR_EPV |
    HAL_CAN_ERROR_BOF |
    HAL_CAN_ERROR_STF |
    HAL_CAN_ERROR_FOR |
    HAL_CAN_ERROR_ACK |
    HAL_CAN_ERROR_BR |
    HAL_CAN_ERROR_BD |
    HAL_CAN_ERROR_CRC |
    HAL_CAN_ERROR_RX_FOV0 |
    HAL_CAN_ERROR_RX_FOV1 |
    HAL_CAN_ERROR_TX_ALST0 |
    HAL_CAN_ERROR_TX_ALST1 |
    HAL_CAN_ERROR_TX_ALST2 |
    HAL_CAN_ERROR_TX_TERR0 |
    HAL_CAN_ERROR_TX_TERR1 |
    HAL_CAN_ERROR_TX_TERR2 |
    HAL_CAN_ERROR_TIMEOUT;
#ifdef HAL_CAN_ERROR_NOT_INITIALIZED
  known_errors |= HAL_CAN_ERROR_NOT_INITIALIZED;
#endif
#ifdef HAL_CAN_ERROR_NOT_READY
  known_errors |= HAL_CAN_ERROR_NOT_READY;
#endif
#ifdef HAL_CAN_ERROR_NOT_STARTED
  known_errors |= HAL_CAN_ERROR_NOT_STARTED;
#endif
#ifdef HAL_CAN_ERROR_PARAM
  known_errors |= HAL_CAN_ERROR_PARAM;
#endif
#ifdef HAL_CAN_ERROR_INTERNAL
  known_errors |= HAL_CAN_ERROR_INTERNAL;
#endif
#ifdef HAL_CAN_ERROR_INVALID_CALLBACK
  known_errors |= HAL_CAN_ERROR_INVALID_CALLBACK;
#endif
  if ((hal_error & ~known_errors) != 0U) {
    error |= PLATFORM_CAN_ERROR_UNKNOWN;
  }
  return error;
}

/**
 * @brief 把项目 CAN 帧描述转换为 HAL 头结构。
 * @param frame 已校验的发送帧；调用方拥有，本层只读取 id、id_type、remote 和 dlc。
 * @param header 输出 HAL 发送头；调用方拥有，非 NULL，函数会先清零再写入。
 * @pre frame 非 NULL，DLC、ID 类型和标识符范围已由公共层校验。
 * @post header 的 DLC、IDE、RTR、StdId/ExtId 与 frame 一致，TransmitGlobalTime 被禁用。
 * @note 内部辅助函数，不处理 data 数组、不校验 DLC 或 ID 范围，也不分配动态内存。
 */
static void platform_can_to_hal_header(
  const platform_can_frame_t *frame,
  CAN_TxHeaderTypeDef *header
) {
  memset(header, 0, sizeof(*header));
  header->DLC = frame->dlc;
  header->IDE = frame->id_type == PLATFORM_CAN_ID_EXTENDED
                  ? CAN_ID_EXT
                  : CAN_ID_STD;
  header->RTR = frame->remote ? CAN_RTR_REMOTE : CAN_RTR_DATA;
  header->TransmitGlobalTime = DISABLE;
  if (frame->id_type == PLATFORM_CAN_ID_EXTENDED) {
    header->ExtId = frame->id;
  } else {
    header->StdId = frame->id;
  }
}

/**
 * @brief 把 HAL CAN 头结构转换为项目帧描述。
 * @param header 已读取的 HAL 接收头；调用方拥有，本层只读取。
 * @param frame 输出帧；调用方拥有，非 NULL；函数只写 id、dlc、id_type 和 remote。
 * @pre header 和 frame 非 NULL。
 * @post frame 的字段与 header 一致；data 数组由调用方另行填充，本函数不修改。
 * @note 内部辅助函数，不校验 header 内容，也不分配动态内存。
 */
static void platform_can_from_hal_header(
  const CAN_RxHeaderTypeDef *header,
  platform_can_frame_t *frame
) {
  frame->dlc = (uint8_t)header->DLC;
  frame->id_type = header->IDE == CAN_ID_EXT
                     ? PLATFORM_CAN_ID_EXTENDED
                     : PLATFORM_CAN_ID_STANDARD;
  frame->remote = header->RTR == CAN_RTR_REMOTE;
  frame->id = frame->id_type == PLATFORM_CAN_ID_EXTENDED
                ? header->ExtId
                : header->StdId;
}

/**
 * @brief 等待 CAN 发送邮箱进入就绪状态。
 * @param can_h 已启动实例的 CAN HAL 句柄；非 NULL，由调用方保证有效。
 * @param timeout 超时时间，单位毫秒；0 表示立即检查，PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 至少一个发送邮箱已空闲。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 到期前没有邮箱空闲。
 * @pre 在任务上下文调用；同一邮箱集合只允许一个等待者，避免并发占用。
 * @post 只轮询空闲级别，不写入邮箱、不启动发送，也不修改错误快照。
 * @note 轮询不会主动休眠；不得在中断上下文调用。
 */
static platform_status_t platform_can_wait_tx_mailbox(
  CAN_HandleTypeDef *can_h,
  uint32_t timeout
) {
  const uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetTxMailboxesFreeLevel(can_h) == 0U) {
    if (
      timeout != PLATFORM_TIMEOUT_INFINITE &&
      (HAL_GetTick() - start) >= timeout
    ) {
      return PLATFORM_STATUS_TIMEOUT;
    }
  }
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 等待 CAN 接收 FIFO 进入就绪状态。
 * @param can_h 已启动实例的 CAN HAL 句柄；非 NULL，由调用方保证有效。
 * @param fifo HAL FIFO 编号，取 CAN_RX_FIFO0 或 CAN_RX_FIFO1。
 * @param timeout 超时时间，单位毫秒；0 表示立即检查，PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 指定 FIFO 中至少有一帧。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 到期前没有帧到达。
 * @pre 在任务上下文调用；fifo 必须已由上层校验为 HAL FIFO 常量。
 * @post 只轮询填充级别，不读取或消费报文，也不修改错误快照。
 * @note 轮询不会主动休眠；不得在中断上下文调用。
 */
static platform_status_t platform_can_wait_rx(
  CAN_HandleTypeDef *can_h,
  uint32_t fifo,
  uint32_t timeout
) {
  const uint32_t start = HAL_GetTick();
  while (HAL_CAN_GetRxFifoFillLevel(can_h, fifo) == 0U) {
    if (
      timeout != PLATFORM_TIMEOUT_INFINITE &&
      (HAL_GetTick() - start) >= timeout
    ) {
      return PLATFORM_STATUS_TIMEOUT;
    }
  }
  return PLATFORM_STATUS_OK;
}

/**
 * @brief 查询 CAN 实例是否已映射到板级启用的 HAL 句柄。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @retval true 实例已启用且映射到 HAL 句柄。
 * @retval false 枚举值越界、实例未启用或缺少 HAL 句柄。
 * @post 只执行映射查询，不访问 CAN 外设、不修改错误快照。
 * @note 可在任务或中断上下文调用；本查询不阻塞且不分配动态内存。
 */
bool platform_can_backend_is_valid(platform_can_instance_t can) {
  return can == PLATFORM_CAN_1 && platform_can_to_hal(can) != NULL;
}

/**
 * @brief 把一个 32 位 CAN 过滤器组配置写入硬件。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param config 过滤器配置；调用方拥有对象，本层只读取而不保存指针。
 * @retval PLATFORM_STATUS_OK 过滤器组已按配置写入。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或过滤器组编号不适用于该实例。
 * @retval PLATFORM_STATUS_ERROR HAL 配置过滤器失败。
 * @pre 必须在 Platform_CAN_Start() 之前调用；config 非 NULL，mode、fifo、id_type、id、id2/mask
 *      已由公共层校验。
 * @post 成功返回后 bank 的 32 位掩码或列表规则生效；config 不保留，调用后可释放其存储。
 * @note 标识符字段是 CAN ID 位，不是 HAL 寄存器原始值；CAN1 使用 bank 0..13，启用 CAN2 时使用 14..27。
 */
platform_status_t platform_can_backend_configure_filter(
  platform_can_instance_t can,
  const platform_can_filter_config_t *config
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  if (can_h == NULL || config->bank > 13U) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }

  const uint32_t id = platform_can_filter_identifier(config, config->id);
  const uint32_t secondary =
    config->mode == PLATFORM_CAN_FILTER_LIST
      ? platform_can_filter_identifier(config, config->id2)
      : platform_can_filter_mask(config);

  CAN_FilterTypeDef filter = {0};
  filter.FilterIdHigh = (uint16_t)(id >> 16U);
  filter.FilterIdLow = (uint16_t)(id & 0xFFFFU);
  filter.FilterMaskIdHigh = (uint16_t)(secondary >> 16U);
  filter.FilterMaskIdLow = (uint16_t)(secondary & 0xFFFFU);
  filter.FilterFIFOAssignment = config->fifo == PLATFORM_CAN_FIFO_0
                                  ? CAN_FILTER_FIFO0
                                  : CAN_FILTER_FIFO1;
  filter.FilterBank = config->bank;
  filter.FilterMode = config->mode == PLATFORM_CAN_FILTER_MASK
                        ? CAN_FILTERMODE_IDMASK
                        : CAN_FILTERMODE_IDLIST;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterActivation = config->enabled ? ENABLE : DISABLE;
  filter.SlaveStartFilterBank = 14U;
  return platform_from_hal_status(HAL_CAN_ConfigFilter(can_h, &filter));
}

/**
 * @brief 启动 CAN 外设并允许收发。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @retval PLATFORM_STATUS_OK CAN 已进入运行状态。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件启动失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @pre MX_CANx_Init() 已完成，过滤器已按需要在启动前配置。
 * @post 成功后 CAN 可收发；bus-off 不会由本层自动恢复，调用方须显式 Stop、重配并 Start。
 * @note 本函数不执行 CubeMX 结构性初始化，也不清理由 HAL 记录的错误快照。
 */
platform_status_t platform_can_backend_start(
  platform_can_instance_t can
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_Start(can_h));
}

/**
 * @brief 停止 CAN 外设并终止新的硬件收发活动。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @retval PLATFORM_STATUS_OK CAN 已停止。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设仍处于忙状态。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件停止失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例越界、未启用或没有 HAL 句柄。
 * @post 成功后不再接受新的邮箱发送或 FIFO 接收；已有中断事件可能仍在分发途中。
 * @note 不等待在途回调结束，不释放缓冲区，也不修改 CubeMX 配置；不分配动态内存。
 */
platform_status_t platform_can_backend_stop(
  platform_can_instance_t can
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_Stop(can_h));
}

/**
 * @brief 等待空闲邮箱后将一帧经典 CAN 加入发送队列。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param frame 待发送帧；调用方拥有，非 NULL；id、id_type、dlc 和 remote 必须由公共层校验，
 *              data 至少包含 dlc 字节。
 * @param timeout 等待空闲邮箱的超时时间，单位毫秒；0 表示立即检查，
 *                PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 帧已复制到空闲发送邮箱。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_TIMEOUT 没有邮箱在 timeout 内变为空闲。
 * @retval PLATFORM_STATUS_ERROR HAL 或硬件拒绝发送请求。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CAN 已启动，frame 非 NULL 且字段已由公共层校验，本函数在任务上下文调用。
 * @post 返回 OK 只表示帧已入队，不表示总线发送完成；发送完成后由中断事件报告。
 * @note 实现轮询邮箱且不主动休眠；帧数据在入队返回后不再被本层引用。
 */
platform_status_t platform_can_backend_transmit(
  platform_can_instance_t can,
  const platform_can_frame_t *frame,
  uint32_t timeout
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  if (can_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  platform_status_t status =
    platform_can_wait_tx_mailbox(can_h, timeout);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }
  CAN_TxHeaderTypeDef header;
  platform_can_to_hal_header(frame, &header);
  uint32_t mailbox = 0U;
  return platform_from_hal_status(
    HAL_CAN_AddTxMessage(can_h, &header, frame->data, &mailbox)
  );
}

/**
 * @brief 阻塞等待指定 FIFO 并读取一帧经典 CAN。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param fifo 接收 FIFO，取 PLATFORM_CAN_FIFO_0 或 PLATFORM_CAN_FIFO_1。
 * @param frame 目标帧；调用方拥有，非 NULL；成功时覆盖其中的 id、dlc、id_type、remote 和 data。
 * @param timeout 等待 FIFO 非空的超时时间，单位毫秒；0 表示立即检查，
 *                PLATFORM_TIMEOUT_INFINITE 表示无限等待。
 * @retval PLATFORM_STATUS_OK 一帧已复制到 frame。
 * @retval PLATFORM_STATUS_TIMEOUT 在 timeout 内没有可接收帧，frame 保持不变。
 * @retval PLATFORM_STATUS_ERROR HAL 读取 FIFO 失败。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CAN 已启动，frame 非 NULL，fifo 为 PLATFORM_CAN_FIFO_0/1，且本函数在任务上下文调用。
 * @post 成功时消费一个 FIFO 报文；HAL 读取失败时 frame 内容不保证完整。
 * @note 实现轮询 FIFO 且不主动休眠；不得在中断上下文调用。
 */
platform_status_t platform_can_backend_receive(
  platform_can_instance_t can,
  platform_can_fifo_t fifo,
  platform_can_frame_t *frame,
  uint32_t timeout
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  if (can_h == NULL) {
    return PLATFORM_STATUS_INVALID_ARGUMENT;
  }
  const uint32_t hal_fifo = fifo == PLATFORM_CAN_FIFO_0
                              ? CAN_RX_FIFO0
                              : CAN_RX_FIFO1;
  platform_status_t status = platform_can_wait_rx(can_h, hal_fifo, timeout);
  if (status != PLATFORM_STATUS_OK) {
    return status;
  }
  CAN_RxHeaderTypeDef header;
  memset(frame, 0, sizeof(*frame));
  status = platform_from_hal_status(
    HAL_CAN_GetRxMessage(can_h, hal_fifo, &header, frame->data)
  );
  if (status == PLATFORM_STATUS_OK) {
    platform_can_from_hal_header(&header, frame);
  }
  return status;
}

/**
 * @brief 启用选定的 CAN 收发、FIFO 或错误中断通知。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param notifications platform_can_notification_t 的非零位掩码；公共层保证只含已定义通知位。
 * @retval PLATFORM_STATUS_OK 指定通知已启用。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝通知配置。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre CAN 和 NVIC 已由 CubeMX 初始化；notifications 非零且仅含已定义通知位；如需事件，
 *      公共层回调已在初始化阶段注册。
 * @post 成功后相应 HAL 中断回调可被触发；回调运行在中断上下文，事件及帧仅在回调期间有效。
 * @note notifications 的无定义位被忽略，不会扩展为任意 HAL 中断；本函数不分配动态内存。
 */
platform_status_t platform_can_backend_activate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_ActivateNotification(
                       can_h,
                       platform_can_notifications(notifications)
                     ));
}

/**
 * @brief 停用选定的 CAN 收发、FIFO 或错误中断通知。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @param notifications platform_can_notification_t 的非零位掩码；公共层保证只含已定义通知位。
 * @retval PLATFORM_STATUS_OK 指定通知已停用。
 * @retval PLATFORM_STATUS_BUSY HAL 报告外设忙。
 * @retval PLATFORM_STATUS_ERROR HAL 拒绝通知配置。
 * @retval PLATFORM_STATUS_INVALID_ARGUMENT 实例非法或没有 HAL 句柄。
 * @pre notifications 非零且仅含已定义通知位；公共层已完成实例和掩码校验。
 * @post 成功后不再产生对应的新中断，但已挂起的中断仍可能执行一次回调。
 * @note 不等待在途回调结束；本函数不分配动态内存。
 */
platform_status_t platform_can_backend_deactivate_notification(
  platform_can_instance_t can,
  uint32_t notifications
) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_STATUS_INVALID_ARGUMENT
                   : platform_from_hal_status(HAL_CAN_DeactivateNotification(
                       can_h,
                       platform_can_notifications(notifications)
                     ));
}

/**
 * @brief 读取并映射 CAN 后端当前错误位。
 * @param can CAN 实例枚举值；有效值由 platform_can_instance_t 定义。
 * @return platform_can_error_t 位掩码；可同时包含多个错误位，NONE 表示 HAL 未报告错误，
 *         实例非法时返回 PARAMETER。
 * @post 只读取 HAL 错误快照，不清除错误、不改变总线状态，也不停止外设。
 * @note 可在任务或中断上下文调用；读取不会触发 bus-off 恢复。
 */
uint32_t platform_can_backend_get_error(platform_can_instance_t can) {
  CAN_HandleTypeDef *can_h = platform_can_to_hal(can);
  return can_h == NULL ? PLATFORM_CAN_ERROR_PARAMETER
                   : platform_can_map_error(HAL_CAN_GetError(can_h));
}

/**
 * @brief 分发 CAN 发送完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @param mailbox 发送邮箱编号，范围 0..2，与 HAL 回调对应。
 * @param type 要分发的事件类型，通常为 TX_COMPLETE 或 TX_ABORT_COMPLETE。
 * @pre mailbox 处于 0..2，type 为已定义事件类型；can_h 允许为 NULL。
 * @post 句柄可识别时构造事件并同步调用平台分发函数；否则无操作，不修改邮箱或错误状态。
 * @note 由 HAL 中断上下文调用；事件只在回调期间有效，不得阻塞或分配内存。
 */
static void platform_can_dispatch_tx(
  CAN_HandleTypeDef *can_h,
  uint8_t mailbox,
  platform_can_event_type_t type
) {
  platform_can_instance_t can;
  if (!platform_hal_to_can(can_h, &can)) {
    return;
  }
  const platform_can_event_t event = {
    .type = type,
    .mailbox = mailbox,
  };
  platform_can_dispatch_event(can, &event);
}

/**
 * @brief 分发 CAN 接收完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @param fifo 接收 FIFO，取 PLATFORM_CAN_FIFO_0 或 PLATFORM_CAN_FIFO_1。
 * @param type 待分发事件类型，通常为 RX_PENDING 或 RX_FULL。
 * @pre fifo 为 PLATFORM_CAN_FIFO_0/1，type 为已定义事件类型；can_h 允许为 NULL。
 * @post 句柄可识别时消费一帧并同步分发事件；HAL 读取失败时改为分发 ERROR 并携带当前错误位。
 * @note 由 HAL 中断上下文调用；成功读取时会消耗 FIFO 报文，事件帧只在回调期间有效。
 */
static void platform_can_dispatch_rx(
  CAN_HandleTypeDef *can_h,
  platform_can_fifo_t fifo,
  platform_can_event_type_t type
) {
  platform_can_instance_t can;
  if (!platform_hal_to_can(can_h, &can)) {
    return;
  }
  const uint32_t hal_fifo = fifo == PLATFORM_CAN_FIFO_0
                              ? CAN_RX_FIFO0
                              : CAN_RX_FIFO1;
  CAN_RxHeaderTypeDef header;
  platform_can_event_t event = {
    .type = type,
    .fifo = fifo,
  };
  if (
    HAL_CAN_GetRxMessage(can_h, hal_fifo, &header, event.frame.data) == HAL_OK
  ) {
    platform_can_from_hal_header(&header, &event.frame);
  } else {
    event.type = PLATFORM_CAN_EVENT_ERROR;
    event.error = platform_can_backend_get_error(can);
  }
  platform_can_dispatch_event(can, &event);
}

/**
 * @brief 分发 CAN 邮箱 0 发送完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 TX_COMPLETE 事件，mailbox 字段为 0；不自动重发或释放缓冲区。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 0U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

/**
 * @brief 分发 CAN 邮箱 1 发送完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 TX_COMPLETE 事件，mailbox 字段为 1；不自动重发或释放缓冲区。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 1U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

/**
 * @brief 分发 CAN 邮箱 2 发送完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 TX_COMPLETE 事件，mailbox 字段为 2；不自动重发或释放缓冲区。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 2U, PLATFORM_CAN_EVENT_TX_COMPLETE);
}

/**
 * @brief 分发 CAN 邮箱 0 中止完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 TX_ABORT_COMPLETE 事件，mailbox 字段为 0；不清除错误快照。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 0U, PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE);
}

/**
 * @brief 分发 CAN 邮箱 1 中止完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 TX_ABORT_COMPLETE 事件，mailbox 字段为 1；不清除错误快照。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 1U, PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE);
}

/**
 * @brief 分发 CAN 邮箱 2 中止完成事件。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 TX_ABORT_COMPLETE 事件，mailbox 字段为 2；不清除错误快照。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时操作。
 */
void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_tx(can_h, 2U, PLATFORM_CAN_EVENT_TX_ABORT_COMPLETE);
}

/**
 * @brief 处理 CAN FIFO0 待接收报文并转发通知。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时从 FIFO0 读取并分发一帧 RX_PENDING；读取失败时改为分发 ERROR。
 *       回调返回后可接收下一帧，但本函数不自动重新配置中断。
 * @note 由 HAL 中断上下文调用；帧数据只在回调期间有效，回调不得阻塞或分配内存。
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_0,
    PLATFORM_CAN_EVENT_RX_PENDING
  );
}

/**
 * @brief 处理 CAN FIFO1 待接收报文并转发通知。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时从 FIFO1 读取并分发一帧 RX_PENDING；读取失败时改为分发 ERROR。
 *       回调返回后可接收下一帧，但本函数不自动重新配置中断。
 * @note 由 HAL 中断上下文调用；帧数据只在回调期间有效，回调不得阻塞或分配内存。
 */
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_1,
    PLATFORM_CAN_EVENT_RX_PENDING
  );
}

/**
 * @brief 处理 CAN FIFO0 满事件并转发通知。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时尝试读取并分发 FIFO0 的 RX_FULL 事件；读取失败时改为分发 ERROR。
 * @note 由 HAL 中断上下文调用；满事件不保证缓冲区仍可继续接收，恢复由上层处理。
 */
void HAL_CAN_RxFifo0FullCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_0,
    PLATFORM_CAN_EVENT_RX_FULL
  );
}

/**
 * @brief 处理 CAN FIFO1 满事件并转发通知。
 * @param can_h 触发中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时尝试读取并分发 FIFO1 的 RX_FULL 事件；读取失败时改为分发 ERROR。
 * @note 由 HAL 中断上下文调用；满事件不保证缓冲区仍可继续接收，恢复由上层处理。
 */
void HAL_CAN_RxFifo1FullCallback(CAN_HandleTypeDef *can_h) {
  platform_can_dispatch_rx(
    can_h,
    PLATFORM_CAN_FIFO_1,
    PLATFORM_CAN_EVENT_RX_FULL
  );
}

/**
 * @brief 记录并分发 CAN 错误事件。
 * @param can_h 触发错误中断的 CAN HAL 句柄；允许为 NULL，未知句柄时忽略。
 * @post 句柄可识别时分发 ERROR 事件，携带当前映射后的 CAN 错误位；不清除 HAL 错误，
 *       也不执行 bus-off 恢复。
 * @note 由 HAL 中断上下文调用；平台回调不得阻塞、分配内存或执行长耗时处理。
 */
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *can_h) {
  platform_can_instance_t can;
  if (!platform_hal_to_can(can_h, &can)) {
    return;
  }
  const platform_can_event_t event = {
    .type = PLATFORM_CAN_EVENT_ERROR,
    .error = platform_can_backend_get_error(can),
  };
  platform_can_dispatch_event(can, &event);
}
