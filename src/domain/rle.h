#pragma once

/**
 * @file rle.h
 * @brief 严格且有界的 RLE 编解码与类型化错误。
 */

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "domain/grid.h"
#include "domain/pattern.h"
#include "domain/rule.h"

namespace lifegame::domain
{

/// @brief 稳定的 RLE 错误码；领域层不返回面向用户的文案。
enum class RleError
{
    None = 0,           ///< 无错误。
    EmptyBody,          ///< 正文里没有任何细胞。
    InvalidHeader,      ///< x/y 或规则头部无法解析。
    InvalidCharacter,   ///< 出现 RLE 字母表之外的字符。
    RunTooLong,         ///< 游程计数超出范围。
    TooLarge,           ///< 解码面积或输入长度超出上限。
    InvalidRule,        ///< 头部规则不是合法的 B/S 记法。
    MissingTerminator   ///< 正文结束时缺少 '!' 终止符。
};

/// @brief 错误码的固定拼写，用于日志。
/// @param error 错误码。
/// @return 静态名称；枚举外的取值返回 "Unknown"。
constexpr const char* rleErrorName(RleError error) noexcept
{
    switch (error)
    {
    case RleError::None:
        return "None";
    case RleError::EmptyBody:
        return "EmptyBody";
    case RleError::InvalidHeader:
        return "InvalidHeader";
    case RleError::InvalidCharacter:
        return "InvalidCharacter";
    case RleError::RunTooLong:
        return "RunTooLong";
    case RleError::TooLarge:
        return "TooLarge";
    case RleError::InvalidRule:
        return "InvalidRule";
    case RleError::MissingTerminator:
        return "MissingTerminator";
    }
    return "Unknown";
}

/// @brief 带源码位置的机器可读 RLE 失败信息。
struct RleErrorInfo
{
    RleError code = RleError::None; ///< 稳定的错误码。
    std::size_t position = 0;       ///< 原始字节偏移（从 0 起）。
    int line = 1;                   ///< 源码行号（从 1 起）。
    int column = 1;                 ///< 源码列号（从 1 起）。
};

/// @brief 成功解码的 RLE 文档。
struct DecodedDocument
{
    Pattern pattern;          ///< 解码出的细胞与尺寸。
    std::string title;        ///< 可选的 `#N` 标题。
    std::string description;  ///< 可选的 `#C` 描述（只取首行）。
    std::string rule;         ///< 规范 B/S 记法，缺失时为空。
};

/// @brief decodeRle() 的结果：文档或类型化错误。
struct RleDecodeResult
{
    std::optional<DecodedDocument> document; ///< 成功时的解码文档。
    RleErrorInfo error;                      ///< 失败时的错误细节。
};

/**
 * @brief 解码 RLE 文本。
 *
 * 解析器刻意严格且有界：畸形或恶意的文件既不能让行列计数溢出，也不能让
 * 解码器写到缓冲区之外。
 *
 * @param text RLE 源文本。
 * @return 解码文档，或带行列号的类型化错误。
 */
RleDecodeResult decodeRle(std::string_view text);

/**
 * @brief 把棋盘编码成 RLE。
 * @param board 待编码的棋盘。
 * @param rule 写入头部的规则。
 * @param title 可选的 `#N` 标题。
 * @return 含头部与 '!' 终止符的完整 RLE 文本。
 */
std::string encodeRle(const Grid& board, const Rule& rule, std::string_view title = {});

} // namespace lifegame::domain
