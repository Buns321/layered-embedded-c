/**
 * @file sys_text.h
 * @brief 无 locale 依赖的 ASCII 文本比较、原地空白切分与有限浮点解析。
 * @details 属于 Layer 2（Framework/Core），避免引入标准库 locale/strtof 带来的体积与
 *          不确定性，供串口命令行与参数解析模块安全使用；零动态内存分配。
 * @addtogroup Framework_Core
 * @{
 */

#ifndef LAYERED_CAR_SYS_TEXT_H
#define LAYERED_CAR_SYS_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 忽略 ASCII 字母大小写比较两段 NUL 结尾字符串是否完全相等。
 * @param[in] left  左侧 NUL 结尾字符串指针（允许为 NULL，为 NULL 时返回 false）。
 * @param[in] right 右侧 NUL 结尾字符串指针（允许为 NULL，为 NULL 时返回 false）。
 * @return 两字符串忽略 ASCII 大小写后是否完全一致。
 */
bool Sys_Text_EqualsIgnoreCase(const char *left, const char *right);

/**
 * @brief 原地按 SPACE/TAB/CR 切分字符串并输出 token 视图。
 * @param[in,out] line           待切分的可写 NUL 结尾字符串（切分过程会原地写入 '\\0'）。
 * @param[out]    tokens         接收各 token 首地址的指针数组（不可为 NULL）。
 * @param[in]     token_capacity tokens 数组最大容量（单位：个，须 > 0）。
 * @param[out]    token_count    输出实际切分出的 token 数量（仅成功时写入，不可为 NULL）。
 * @return 切分是否成功。
 * @retval true  至少切分出 1 个 token 且未超出 token_capacity。
 * @retval false 参数非法、空行/纯空白行或 token 数量超出容量上限。
 */
bool Sys_Text_SplitWhitespace(char *line, char **tokens, uint32_t token_capacity,
                              uint32_t *token_count);

/**
 * @brief 严格解析 NUL 结尾有限十进制单精度浮点数。
 * @param[in]  text  待解析的十进制数字字符串（不支持首尾空白及 NaN/Inf 文本，不可为 NULL）。
 * @param[out] value 解析成功后输出的单精度浮点结果（不可为 NULL）。
 * @return 解析是否成功且结果为有限浮点数。
 * @retval true  格式合法且结果满足 SYS_ISFINITE。
 * @retval false 指针为空、格式非法、存在多余尾随字符或溢出为非有限值。
 */
bool Sys_Text_ParseFiniteF32(const char *text, float *value);

/** @} */

#endif /* LAYERED_CAR_SYS_TEXT_H */
