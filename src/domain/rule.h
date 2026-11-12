#pragma once

/**
 * @file rule.h
 * @brief 生命类规则的掩码、解析与预设。
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lifegame::domain
{

struct RuleParseResult;
class Rule;

/// @brief B/S 记法解析失败的类型化错误。
enum class RuleParseError
{
    None = 0,          ///< 无错误。
    Empty,             ///< 输入为空。
    MissingSeparator,  ///< 出生集与存活集之间缺少 '/'。
    InvalidCharacter   ///< 字符不在 '0'..'8' 内，或前缀不合法。
};

/// @brief 解析错误的固定拼写，用于日志。
/// @param error 解析错误。
/// @return 静态名称；枚举外的取值返回 "Unknown"。
constexpr const char* ruleParseErrorName(RuleParseError error) noexcept
{
    switch (error)
    {
    case RuleParseError::None:
        return "None";
    case RuleParseError::Empty:
        return "Empty";
    case RuleParseError::MissingSeparator:
        return "MissingSeparator";
    case RuleParseError::InvalidCharacter:
        return "InvalidCharacter";
    }
    return "Unknown";
}

/**
 * @brief B/S 记法表示的生命类规则。
 *
 * 规则存两个 9 位掩码，第 n 位表示“邻居数为 n”。领域类型不含显示名称；
 * 本地化名称属于视图层。
 */
class Rule
{
public:
    /// @brief 构造康威规则 B3/S23。
    constexpr Rule() = default;

    /**
     * @brief 用出生/存活掩码构造规则。
     * @param birth 出生掩码（第 0..8 位）。
     * @param survival 存活掩码（第 0..8 位）。
     */
    constexpr Rule(std::uint16_t birth, std::uint16_t survival) noexcept
        : m_birth(static_cast<std::uint16_t>(birth & 0x01FFu)),
          m_survival(static_cast<std::uint16_t>(survival & 0x01FFu))
    {
    }

    /// @brief 查询出生掩码。
    /// @param neighbours 邻居数。
    /// @return 该邻居数下死细胞会出生时返回 true。
    constexpr bool births(int neighbours) const noexcept
    {
        return neighbours >= 0 && neighbours <= 8 && (m_birth & (1u << neighbours)) != 0;
    }

    /// @brief 查询存活掩码。
    /// @param neighbours 邻居数。
    /// @return 该邻居数下活细胞会存活时返回 true。
    constexpr bool survives(int neighbours) const noexcept
    {
        return neighbours >= 0 && neighbours <= 8 && (m_survival & (1u << neighbours)) != 0;
    }

    /// @brief 原始出生掩码。
    /// @return 出生掩码（第 0..8 位）。
    constexpr std::uint16_t birthMask() const noexcept
    {
        return m_birth;
    }

    /// @brief 原始存活掩码。
    /// @return 存活掩码（第 0..8 位）。
    constexpr std::uint16_t survivalMask() const noexcept
    {
        return m_survival;
    }

    /// @brief 输出规范记法。
    /// @return 规范记法，例如 "B3/S23"。
    std::string notation() const;

    /**
     * @brief 解析 B/S 记法。
     *
     * 同时接受 "B3/S23" 与 "3/23" 两种写法，忽略大小写与首尾空白；出生集
     * 或存活集为空（如 "B2/S"）也是合法的。失败时返回类型化错误码。
     * @param text 待解析的记法。
     * @return 解析出的规则，或类型化错误。
     */
    static RuleParseResult parse(std::string_view text);

    /// @brief 比较两个掩码。
    /// @param lhs 左操作数。
    /// @param rhs 右操作数。
    /// @return 两个掩码都相同时返回 true。
    friend constexpr bool operator==(const Rule& lhs, const Rule& rhs) noexcept
    {
        return lhs.m_birth == rhs.m_birth && lhs.m_survival == rhs.m_survival;
    }

    /// @brief 比较两个掩码。
    /// @param lhs 左操作数。
    /// @param rhs 右操作数。
    /// @return 任一掩码不同时返回 true。
    friend constexpr bool operator!=(const Rule& lhs, const Rule& rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    std::uint16_t m_birth = static_cast<std::uint16_t>(1u << 3); ///< 出生掩码；第 n 位表示 n 个邻居时出生。
    std::uint16_t m_survival = static_cast<std::uint16_t>((1u << 2) | (1u << 3)); ///< 存活掩码。
};

/// @brief Rule::parse() 的结果。
struct RuleParseResult
{
    RuleParseError error = RuleParseError::None; ///< 解析错误；成功时为 None。
    Rule rule;                                   ///< 解析成功时的规则。
};

/// @brief 带稳定、非本地化标识的内置规则。
struct RulePreset
{
    std::string_view id; ///< 稳定的预设 ID。
    Rule rule;           ///< 规则值。
};

/// @brief 按菜单顺序排列的内置规则。
/// @return 按菜单顺序排列的内置规则。
const std::vector<RulePreset>& builtinRulePresets();

/// @brief 按稳定 ID 查找内置规则。
/// @param id 稳定的预设 ID。
/// @return 匹配的预设；找不到时返回 nullptr。
const RulePreset* findRulePreset(std::string_view id);

/// @brief 按规范记法查找内置规则。
/// @param notation 规范 B/S 记法。
/// @return 匹配的预设；找不到时返回 nullptr。
const RulePreset* findRulePresetByNotation(std::string_view notation);

} // namespace lifegame::domain
