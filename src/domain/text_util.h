#pragma once

/**
 * @file text_util.h
 * @brief 领域解析器共用的小型内部文本辅助函数。
 */

#include <cctype>
#include <string_view>

namespace lifegame::domain::detail
{

/// @brief 去掉首尾的 ASCII 空白。
/// @param text 输入视图。
/// @return 指向同一缓冲区、去掉空白后的视图。
inline std::string_view trim(std::string_view text)
{
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())) != 0)
    {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0)
    {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace lifegame::domain::detail
