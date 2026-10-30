//
// 由 Buns_ 创建于 2026/8/26。
//

/**
 * @file uart_base.c
 * @brief UART 抽象接口实现。
 * @ingroup drivers_base
 */

#include "uart_base.h"
#include <assert.h>

/**
 * @brief 绑定 UART 名称与具体收发操作表。
 * @param uart_h 调用方持有的 UART 基类存储；不可为 NULL，所有权不转移。
 * @param name 只读调试名称，生命周期必须覆盖 UART 对象；不可为 NULL。
 * @param ops 静态操作表；不可为 NULL，具体收发操作在使用时按需校验。
 * @pre uart_h 尚未并发发布，name 与 ops 在对象使用期间保持有效。
 * @post 保存绑定，软件使能状态置为 DEV_UART_ON，并清空全部回调与 context。
 * @note 仅限初始化阶段调用；不初始化或启动硬件、不注册 Platform 回调、不加锁。
 */
void Dev_UART_Base_Init(uart_base_t *uart_h, const char *name, const uart_ops_t *ops) {
  assert(uart_h && name && ops);
  uart_h->ops              = ops;
  uart_h->name             = name;
  uart_h->is_uart_enabled = DEV_UART_ON;
  uart_h->callbacks        = (dev_uart_callbacks_t){0};
}

/**
 * @brief 读取 UART 软件使能状态。
 * @param uart_h 已初始化的只读 UART 句柄；不可为 NULL，所有权不转移。
 * @retval DEV_UART_ON 软件使能状态为开启。
 * @retval DEV_UART_OFF 软件使能状态为关闭。
 * @pre uart_h 已通过 Dev_UART_Base_Init 初始化。
 * @post 不访问硬件、不改变传输状态或回调绑定。
 * @note 该值不代表 Platform 外设当前是否可收发，也不提供数据新鲜度。
 */
dev_uart_state_t Dev_UART_Base_ReadStatus(const uart_base_t *uart_h) {
  assert((uart_h));
  return uart_h->is_uart_enabled;
}

/**
 * @brief 阻塞接收指定字节数。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param buf 调用方持有的接收缓冲区；不可为 NULL，必须可写 size 字节。
 * @param size 请求接收的字节数；必须大于 0，范围受 uint16_t 限制。
 * @param timeout 超时时间，单位 ms；0 与无限等待的语义由具体后端约定。
 * @retval DEV_UART_OK 指定字节已全部接收。
 * @retval DEV_UART_BUSY 传输通道被占用，接收未开始。
 * @retval DEV_UART_TIMEOUT 超时前未完成接收。
 * @retval DEV_UART_ERROR 参数非法或底层收发错误。
 * @pre 已绑定非空 receive 操作，Platform UART 已初始化；调用期间 buf 保持有效。
 * @post 成功时 buf 前 size 字节被写入；失败时已写内容不保证清除，所有权仍归调用方。
 * @warning 该调用会阻塞，禁止在 ISR 调用；同一 UART 的并发操作须由调用方串行化。
 */
dev_uart_status_t Dev_UART_Base_Receive(uart_base_t *uart_h, uint8_t *buf, uint16_t size, uint32_t timeout) {
  assert(uart_h && uart_h->ops && uart_h->ops->receive && buf && size != 0u);
  return uart_h->ops->receive(uart_h, buf, size, timeout);
}

/**
 * @brief 阻塞发送指定字节数。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param data 调用方持有的只读发送缓冲区；不可为 NULL，必须可读 size 字节。
 * @param size 请求发送的字节数；必须大于 0，范围受 uint16_t 限制。
 * @param timeout 超时时间，单位 ms；0 与无限等待的语义由具体后端约定。
 * @retval DEV_UART_OK 指定字节已全部发送。
 * @retval DEV_UART_BUSY 传输通道被占用，发送未开始。
 * @retval DEV_UART_TIMEOUT 超时前未完成发送。
 * @retval DEV_UART_ERROR 参数非法或底层收发错误。
 * @pre 已绑定非空 transmit 操作，Platform UART 已初始化；调用期间 data 保持有效。
 * @post data 内容不被修改，所有权仍归调用方；部分发送失败后不自动重试。
 * @warning 该调用会阻塞，禁止在 ISR 调用；同一 UART 的并发操作须由调用方串行化。
 */
dev_uart_status_t Dev_UART_Base_Transmit(uart_base_t *uart_h, const uint8_t *data, uint16_t size, uint32_t timeout) {
  assert(uart_h && uart_h->ops && uart_h->ops->transmit && data && size != 0u);
  return uart_h->ops->transmit(uart_h, data, size, timeout);
}

/**
 * @brief 启动中断接收。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param pdata 调用方持有的接收缓冲区；不可为 NULL，必须可写 size 字节。
 * @param size 请求接收的字节数；必须大于 0，范围受 uint16_t 限制。
 * @retval DEV_UART_OK 接收已成功挂载。
 * @retval DEV_UART_BUSY UART 已有活动传输或接收。
 * @retval DEV_UART_ERROR 参数非法或底层启动失败。
 * @retval DEV_UART_TIMEOUT 具体后端以超时表示启动失败。
 * @pre 已绑定非空 receive_it 操作，Platform UART 及其中断已初始化。
 * @post 成功返回后中断可写入 pdata；调用方必须保持缓冲区有效到完成、错误或中止回调。
 * @warning Platform 不自动重挂接收，设备驱动需在回调链中重新启动；禁止在 ISR 并发修改同一缓冲。
 */
dev_uart_status_t Dev_UART_ReceiveIT(uart_base_t *uart_h, uint8_t *pdata, uint16_t size) {
  assert(uart_h && uart_h->ops && uart_h->ops->receive_it && pdata && size != 0u);
  return uart_h->ops->receive_it(uart_h, pdata, size);
}

/**
 * @brief 启动中断发送。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param pdata 调用方持有的只读发送缓冲区；不可为 NULL，必须可读 size 字节。
 * @param size 请求发送的字节数；必须大于 0，范围受 uint16_t 限制。
 * @retval DEV_UART_OK 发送已成功挂载。
 * @retval DEV_UART_BUSY UART 已有活动传输。
 * @retval DEV_UART_ERROR 参数非法或底层启动失败。
 * @retval DEV_UART_TIMEOUT 具体后端以超时表示启动失败。
 * @pre 已绑定非空 transmit_it 操作，Platform UART 及其中断已初始化。
 * @post 成功返回后中断可读取 pdata；调用方必须保持缓冲区内容有效到完成、错误或中止回调。
 * @warning 不得在回调前修改或释放 pdata；本层不加锁，同一 UART 的并发启动须串行化。
 */
dev_uart_status_t Dev_UART_TransmitIT(uart_base_t *uart_h, const uint8_t *pdata, uint16_t size){
  assert(uart_h && uart_h->ops && uart_h->ops->transmit_it && pdata && size != 0u);
  return uart_h->ops->transmit_it(uart_h, pdata, size);
}

/**
 * @brief 启动 DMA 接收。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param pdata 调用方持有的接收缓冲区；不可为 NULL，必须可写 size 字节。
 * @param size 请求接收的字节数；必须大于 0，范围受 uint16_t 限制。
 * @retval DEV_UART_OK DMA 接收已成功挂载。
 * @retval DEV_UART_BUSY UART 或 DMA 通道已有活动传输。
 * @retval DEV_UART_ERROR 参数非法或底层启动失败。
 * @retval DEV_UART_TIMEOUT 具体后端以超时表示启动失败。
 * @pre 已绑定非空 receive_dma 操作，Platform UART 与 DMA 已初始化。
 * @post 成功返回后 DMA 可写入 pdata；缓冲区必须有效到完成、半完成、错误或中止回调。
 * @warning 半完成回调只能访问已完成的一半；不得在传输结束前复用缓冲区。
 */
dev_uart_status_t Dev_UART_ReceiveDMA(uart_base_t *uart_h, uint8_t *pdata, uint16_t size) {
  assert(uart_h && uart_h->ops && uart_h->ops->receive_dma && pdata && size != 0u);
  return uart_h->ops->receive_dma(uart_h, pdata, size);
}

/**
 * @brief 启动 DMA 发送。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param pdata 调用方持有的只读发送缓冲区；不可为 NULL，必须可读 size 字节。
 * @param size 请求发送的字节数；必须大于 0，范围受 uint16_t 限制。
 * @retval DEV_UART_OK DMA 发送已成功挂载。
 * @retval DEV_UART_BUSY UART 或 DMA 通道已有活动传输。
 * @retval DEV_UART_ERROR 参数非法或底层启动失败。
 * @retval DEV_UART_TIMEOUT 具体后端以超时表示启动失败。
 * @pre 已绑定非空 transmit_dma 操作，Platform UART 与 DMA 已初始化。
 * @post 成功返回后 DMA 可读取 pdata；缓冲区内容必须保持有效到完成、错误或中止回调。
 * @warning DMA 只持有缓冲区地址，不复制数据；不得在完成前修改或释放 pdata。
 */
dev_uart_status_t Dev_UART_TransmitDMA(uart_base_t *uart_h, const uint8_t *pdata, uint16_t size) {
  assert(uart_h && uart_h->ops && uart_h->ops->transmit_dma && pdata && size != 0u);
  return uart_h->ops->transmit_dma(uart_h, pdata, size);
}

/**
 * @brief 复制 UART 事件回调集合。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @param cb 调用方持有的回调配置；不可为 NULL，结构体内容在返回前被复制。
 * @pre 在启用中断/DMA 或任何回调可能触发之前注册；cb->context 生命周期覆盖全部事件。
 * @post 整张回调表被替换；未设置的回调槽为 NULL，事件到达时按空操作处理。
 * @note 只复制函数指针和 context，不接管其所有权；通常仅在初始化阶段调用。
 * @warning 与正在执行的中断回调并发替换存在数据竞争，调用方须关闭相关中断或串行化。
 */
void Dev_UART_RegisterCallback(uart_base_t *uart_h, const dev_uart_callbacks_t *cb) {
  assert(uart_h && cb);
  uart_h->callbacks = *cb;
}

/**
 * @brief 分发 UART 发送完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform TX complete 事件上下文调用；context 在注册时仍有效。
 * @post 若 tx_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行，回调不得阻塞、分配内存或执行长耗时操作。
 * @warning 本函数本身不加锁，回调与缓冲区的并发生命周期由驱动和调用方保证。
 */
void Dev_UART_Base_OnTxComplete(uart_base_t *uart_h) {
  assert(uart_h);
  if (uart_h->callbacks.tx_complete_cb) {
    uart_h->callbacks.tx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 发送半完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform TX half-complete 事件上下文调用。
 * @post 若 tx_half_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行；回调只能处理已释放给软件的那一半缓冲。
 * @warning 不得阻塞或执行长耗时操作，缓冲区仍须保持有效。
 */
void Dev_UART_Base_OnTxHalfComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.tx_half_complete_cb) {
    uart_h->callbacks.tx_half_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 接收完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform RX complete 事件上下文调用。
 * @post 若 rx_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行；Platform 不会自动重新挂接接收。
 * @warning 回调须自行复制或重新挂接缓冲区，且不得阻塞或长时间占用中断。
 */
void Dev_UART_Base_OnRxComplete(uart_base_t *uart_h) {
  assert(uart_h);
  if (uart_h->callbacks.rx_complete_cb) {
    uart_h->callbacks.rx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 接收半完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform RX half-complete 事件上下文调用。
 * @post 若 rx_half_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行；回调只能访问 DMA 已完成的那一半数据。
 * @warning 不得阻塞或修改未完成的一半缓冲区。
 */
void Dev_UART_Base_OnRxHalfComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.rx_half_complete_cb) {
    uart_h->callbacks.rx_half_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 错误事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform UART error 事件上下文调用。
 * @post 若 err_cb 非空，以 uart_h 和注册 context 调用一次；本层不清除硬件错误标志。
 * @note 通常在 ISR 上下文运行；错误码与恢复动作由具体后端和驱动决定。
 * @warning 回调不得阻塞；是否重新挂接收或中止传输必须由回调链显式处理。
 */
void Dev_UART_Base_OnError(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.err_cb) {
    uart_h->callbacks.err_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 通用中止完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform abort-complete 事件上下文调用。
 * @post 若 abort_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行；调用方可在回调中复用已被中止的缓冲区。
 * @warning 回调不得阻塞或长时间占用中断。
 */
void Dev_UART_Base_OnAbortComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.abort_complete_cb) {
    uart_h->callbacks.abort_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 发送中止完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform abort-TX-complete 事件上下文调用。
 * @post 若 abort_tx_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行；发送缓冲可在收到此事件后释放或复用。
 * @warning 回调不得阻塞，亦不得假定已发送全部请求字节。
 */
void Dev_UART_Base_OnAbortTxComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.abort_tx_complete_cb) {
    uart_h->callbacks.abort_tx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}

/**
 * @brief 分发 UART 接收中止完成事件。
 * @param uart_h 已初始化的 UART 句柄；不可为 NULL，所有权不转移。
 * @pre 由具体 port 在 Platform abort-RX-complete 事件上下文调用。
 * @post 若 abort_rx_complete_cb 非空，以 uart_h 和注册 context 调用一次；否则不执行回调。
 * @note 通常在 ISR 上下文运行；接收缓冲可在收到此事件后释放或复用。
 * @warning 回调不得阻塞，已写入内容不保证完整或有效。
 */
void Dev_UART_Base_OnAbortRxComplete(uart_base_t *uart_h){
  assert(uart_h);
  if (uart_h->callbacks.abort_rx_complete_cb) {
    uart_h->callbacks.abort_rx_complete_cb(uart_h, uart_h->callbacks.context);
  }
}
