/**
 * @file system_text.h
 * @brief 不依赖 locale 的受限文本切分、比较和数值解析。
 * @addtogroup system
 * @{
 * @note 本模块只提供通用文本机制，不解释命令、参数名或业务语义。
 */

#ifndef LAYERED_EMBEDDED_C_SYSTEM_TEXT_H
#define LAYERED_EMBEDDED_C_SYSTEM_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 比较两个 ASCII 文本是否在忽略字母大小写后完全相等。
 * @param left 左侧 NUL 结尾文本；NULL 视为比较失败。
 * @param right 右侧 NUL 结尾文本；NULL 视为比较失败。
 * @return 两侧均非空且逐字符忽略 ASCII 大小写后相等时返回 true，否则返回 false。
 * @pre left 和 right 非 NULL 时必须在调用期间保持可读。
 * @post 不修改任何输入，不访问共享状态，不分配内存。
 * @note 只折叠 ASCII `A..Z` 与 `a..z`；其他字节按原值比较。
 */
bool Sys_Text_EqualsIgnoreCase(const char *left, const char *right);

/**
 * @brief 原地按空白切分文本并输出固定数量的 token 视图。
 * @param line 可写 NUL 结尾文本；函数会插入 NUL 终止符。
 * @param tokens 输出 token 指针数组；不得为 NULL，容量由 token_capacity 指定。
 * @param token_capacity token 数组最大元素数，必须大于 0。
 * @param token_count 输出实际 token 数；不得为 NULL，仅成功时写入。
 * @retval true 至少切出一个 token，且全部 token 均能放入输出数组。
 * @retval false 任一指针为空、容量为 0、文本仅含空白，或 token 数超过容量。
 * @pre line 可写且在返回前保持有效；tokens 和 token_count 可写。
 * @post 成功时 tokens 指向 line 内部片段，连续空白被折叠，首尾空白被移除；
 *       SPACE、TAB 和 CR 被视为空白，LF 不特殊处理。
 * @note 失败时 line 可能已被部分修改，tokens 内容不得依赖；不分配内存。
 */
bool Sys_Text_SplitWhitespace(
  char *line,
  char **tokens,
  uint32_t token_capacity,
  uint32_t *token_count
);

/**
 * @brief 严格解析 NUL 结尾有限十进制单精度浮点数。
 * @param text 待解析文本；不得为 NULL，不允许首尾空白。
 * @param value 输出浮点值；不得为 NULL，仅在返回 true 时写入。
 * @retval true text 完整构成长度合法的有限 float，且至少包含一位尾数数字。
 * @retval false text 为空、格式不完整、含尾随字符、指数不完整或结果非有限。
 * @pre text 和 value 非 NULL，并在调用期间保持有效。
 * @post 成功时 *value 为解析值；失败时 *value 保持不变。
 * @note 接受可选正负号、十进制小数和 `e`/`E` 指数；不依赖 locale，
 *       不分配内存，可在任务或 ISR 上下文调用。
 */
bool Sys_Text_ParseFiniteF32(const char *text, float *value);

/** @} */

#endif //LAYERED_EMBEDDED_C_SYSTEM_TEXT_H
