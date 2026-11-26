/**
 * @file sys_text.h
 * @brief 无 locale 依赖的 ASCII 文本比较、原地空白切分与有限浮点解析。
 * @addtogroup sys_core
 * @{
 */

#ifndef LAYERED_CAR_SYS_TEXT_H
#define LAYERED_CAR_SYS_TEXT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 忽略 ASCII 字母大小写比较两段 NUL 结尾字符串是否完全相等。
 */
bool Sys_Text_EqualsIgnoreCase(const char *left, const char *right);

/**
 * @brief 原地按 SPACE/TAB/CR 切分字符串并输出 token 视图（仅成功时写入 *token_count）。
 */
bool Sys_Text_SplitWhitespace(char *line, char **tokens, uint32_t token_capacity,
                              uint32_t *token_count);

/**
 * @brief 严格解析 NUL 结尾有限十进制单精度浮点数（不支持首尾空白或 NaN/Inf 文本）。
 */
bool Sys_Text_ParseFiniteF32(const char *text, float *value);

/** @} */

#endif /* LAYERED_CAR_SYS_TEXT_H */
