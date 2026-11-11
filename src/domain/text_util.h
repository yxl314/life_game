#pragma once

/**
 * @file text_util.h
 * @brief Small internal text helpers shared by domain parsers.
 */

#include <cctype>
#include <string_view>

namespace lifegame::domain::detail
{

/**
 * @brief Remove leading and trailing ASCII whitespace.
 * @param text Input view.
 * @return Trimmed view into the same buffer.
 */
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
