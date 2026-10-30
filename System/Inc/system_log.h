//
// 由 Buns_ 创建于 2026/10/21。
//

/**
 * @file system_log.h
 * @brief 非阻塞 System 日志门面。
 * @addtogroup system
 * @{
 * @note 默认时间戳来自 STM32 HAL 毫秒时基。其他平台可在编译选项中定义
 *       `SYS_LOG_EXTERNAL_TIME_SOURCE=<函数名>`，该函数返回单调毫秒时间；
 *       也可直接定义 `SYS_LOG_GET_TIME_MS()` 覆盖默认实现。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_LOG_H
#define LAYERED_EMBEDDED_C_SYSTEM_LOG_H

#include "system_log_base.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 日志严重等级。
 */
typedef enum sys_log_level_e {
  SYS_LOG_LEVEL_ERROR = 0,
  SYS_LOG_LEVEL_WARN,
  SYS_LOG_LEVEL_INFO,
  SYS_LOG_LEVEL_DEBUG,
  SYS_LOG_LEVEL_NONE,
} sys_log_level_t;

/** @cond SYS_LOG_INTERNAL */
extern volatile uint32_t g_sys_log_fast_mask;
/** @endcond */

/**
 * @brief 初始化日志门面。
 * @param sink_h 后端对象；传入 NULL 表示关闭输出，但仍保留门面状态可查询。
 * @pre 应在 HAL 毫秒时基初始化完成后、并发写入开始前调用；非 NULL sink_h 的生命周期
 *      必须覆盖日志门面的全部使用期。
 * @post 当前后端已替换为 sink_h，丢弃字节数和竞争计数清零，软件锁释放，快速等级掩码
 *       按当前等级重新生成。
 * @note 不分配动态内存；只在短暂的 PRIMASK 临界区内切换状态，不得与正在进行的日志写入并发调用。
 */
void Sys_Log_Init(sys_log_base_t *sink_h);

/**
 * @brief 替换当前生效的后端。
 * @param sink_h 新的后端对象；传入 NULL 表示关闭输出。
 * @pre 非 NULL sink_h 必须已初始化，且其生命周期覆盖日志门面的剩余使用期。
 * @post 后续日志使用新后端并刷新快速等级掩码；统计计数和当前等级保持不变。
 * @note 替换只保证后续日志使用新后端；已经开始的写入仍可能使用旧后端，
 *       因此旧后端必须在并发写入结束前保持有效。
 * @warning 本函数不等待正在进行的写入，不能用于释放旧后端存储后的热替换。
 */
void Sys_Log_SetSink(sys_log_base_t *sink_h);

/**
 * @brief 设置运行期最低输出等级。
 * @param level 最低严重等级，该等级及更严重等级会被输出；有效值为
 *              SYS_LOG_LEVEL_ERROR..SYS_LOG_LEVEL_NONE。
 * @post 有效等级已保存并刷新快速掩码；超出枚举范围的值按 SYS_LOG_LEVEL_NONE 处理。
 * @note 不分配动态内存；状态切换由短暂 PRIMASK 临界区保护。
 * @warning 无效枚举值会关闭日志输出，且不会返回错误。
 */
void Sys_Log_SetLevel(sys_log_level_t level);

/**
 * @brief 读取当前配置的最低输出等级。
 * @return 当前最低输出等级，取值 SYS_LOG_LEVEL_ERROR..SYS_LOG_LEVEL_NONE。
 * @note 返回在短暂 PRIMASK 临界区内取得的一致快照；尚未设置过时默认 INFO。
 */
sys_log_level_t Sys_Log_GetLevel(void);

/**
 * @brief 判断后端是否已经配置。
 * @retval true 后端已配置，即使当前等级为 NONE。
 * @retval false 后端为 NULL，尚未配置或已关闭输出。
 * @note 只读取快速掩码中的 ready 位，不进入临界区；结果是瞬时快照。
 */
bool Sys_Log_IsReady(void);

/**
 * @brief 判断指定等级当前是否会输出。
 * @param level 待检查的日志等级；越界值不会导致未定义行为。
 * @retval true level 在 ERROR..DEBUG 内，后端已配置且该等级未被过滤。
 * @retval false level 为 NONE/越界，后端未配置，或该等级被当前阈值过滤。
 * @note 该函数位于头文件中，关闭日志时只读取一次快速掩码，不进入临界区。
 *       日志宏先调用本函数，被过滤的日志不会对参数表达式求值。
 */
static inline bool Sys_Log_IsEnabled(sys_log_level_t level) {
  uint32_t level_index = (uint32_t)level;

  if (level_index > (uint32_t)SYS_LOG_LEVEL_DEBUG) {
    return false;
  }

  return (g_sys_log_fast_mask & (1u << level_index)) != 0u;
}

/**
 * @brief 格式化并提交一条日志。
 * @param level 本条日志的严重等级；仅 ERROR..DEBUG 会尝试输出。
 * @param tag 非空只读子系统名称，只在本条日志格式化期间使用。
 * @param format 非空 printf 风格格式字符串，只在本次调用期间使用。
 * @param ... 与 format 匹配的可变参数，由调用方保证类型和数量正确。
 * @pre 在任务上下文调用；tag 和 format 非 NULL，level 在 ERROR..DEBUG 范围内。
 * @post 日志已提交、因门面忙碌被丢弃、因等级/参数无效被忽略，或后端只接受部分字节；
 *       发生丢弃时更新竞争计数或丢弃字节计数。
 * @note 非阻塞；使用 try-lock，格式化或写入期间其他写入者会直接丢弃本条日志。
 *       单条输出缓冲上限为 192 byte，超长内容会被截断；不分配动态内存。
 * @warning 不在 ISR 中调用。常规日志避免格式化浮点数；高频或浮点数据应使用遥测。
 */
void Sys_Log_Write(
  sys_log_level_t level,
  const char *tag,
  const char *format,
  ...
)
#if defined(__GNUC__) || defined(__clang__)
  __attribute__((format(printf, 3, 4)))
#endif
;

/**
 * @brief 不经格式化和加锁，直接提交原始字节。
 * @param data 非空只读原始字节缓冲区，只在本次调用期间访问。
 * @param size 待提交字节数，单位 byte，必须大于 0。
 * @pre 后端至少已设置一次；调用方保证 data 非 NULL 且 size > 0。
 * @post 数据已直接交给当前后端；未接受部分不会计入普通日志丢弃统计。
 * @note 用于 Fault 等最后兜底路径。该接口不参与日志门面互斥，调用方必须
 *       保证不会与普通写入并发，或为紧急路径提供独立后端缓冲区。
 * @warning 与普通写入并发时可能破坏后端缓冲区的原子性；后端缺失或参数无效时静默丢弃。
 */
void Sys_Log_EmergencyRaw(const void *data, uint32_t size);

/**
 * @brief 当前后端拒绝的字节数。
 * @return 自最近一次 Sys_Log_Init 起累计未接受的普通日志字节数，单位 byte；
 *         32 位无符号计数溢出后回绕。
 * @note 在短暂 PRIMASK 临界区内读取一致快照；不包含 Sys_Log_EmergencyRaw 的丢弃量。
 */
uint32_t Sys_Log_GetDropBytes(void);

/**
 * @brief 因其他写入者占用日志门面而被拒绝的日志条数。
 * @return 自最近一次 Sys_Log_Init 起因 try-lock 失败而丢弃的 Sys_Log_Write 调用条数；
 *         32 位无符号计数溢出后回绕。
 * @note 在短暂 PRIMASK 临界区内读取一致快照；参数无效或等级过滤不计入该计数。
 */
uint32_t Sys_Log_GetContentionCount(void);

/**
 * @brief 按 ERROR 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_ERROR(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_ERROR)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_ERROR, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)
/**
 * @brief 按 WARN 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_WARN(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_WARN)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_WARN, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)
/**
 * @brief 按 INFO 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_INFO(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_INFO)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_INFO, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)
/**
 * @brief 按 DEBUG 等级提交一条可被过滤的日志。
 * @param tag 静态生命周期的简短子系统名称。
 * @param format printf 风格格式字符串。
 * @param ... 与 format 匹配的可选参数；等级被过滤时不求值。
 * @note 仅允许任务上下文调用，不得在 ISR 中使用。
 */
#define SYS_LOG_DEBUG(tag, format, ...) \
  do { \
    if (Sys_Log_IsEnabled(SYS_LOG_LEVEL_DEBUG)) { \
      Sys_Log_Write(SYS_LOG_LEVEL_DEBUG, tag, format, ##__VA_ARGS__); \
    } \
  } while (0)

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_LOG_H
