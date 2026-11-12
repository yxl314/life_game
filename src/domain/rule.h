#pragma once

/**
 * @file rule.h
 * @brief Life-like rule masks, parsing and presets.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace lifegame::domain
{

struct RuleParseResult;
class Rule;

/// @brief Typed B/S notation parse failures.
enum class RuleParseError
{
    None = 0,          ///< No error.
    Empty,             ///< Empty input.
    MissingSeparator,  ///< Missing '/' between birth and survival sets.
    InvalidCharacter   ///< Character outside '0'..'8' or bad prefix.
};

/// @brief Stable spelling of a parse error for logs.
///
/// @param error Parse error.
/// @return Static name; "Unknown" for a value outside the enumeration.
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
 * @brief A Life-like rule in B/S notation.
 *
 * The rule stores two 9-bit masks. Bit n means "neighbour count n".
 * The domain type carries no display name; localized names live in the view.
 */
class Rule
{
public:
    /// @brief Construct Conway's rule, B3/S23.
    constexpr Rule() = default;

    /**
     * @brief Construct a rule from birth/survival masks.
     * @param birth Birth mask (bits 0..8).
     * @param survival Survival mask (bits 0..8).
     */
    constexpr Rule(std::uint16_t birth, std::uint16_t survival) noexcept
        : m_birth(static_cast<std::uint16_t>(birth & 0x01FFu)),
          m_survival(static_cast<std::uint16_t>(survival & 0x01FFu))
    {
    }

    /// @brief Query the birth mask.
    ///
    /// @param neighbours Neighbour count.
    /// @return True when a dead cell is born with this count.
    constexpr bool births(int neighbours) const noexcept
    {
        return neighbours >= 0 && neighbours <= 8 && (m_birth & (1u << neighbours)) != 0;
    }

    /// @brief Query the survival mask.
    ///
    /// @param neighbours Neighbour count.
    /// @return True when an alive cell survives with this count.
    constexpr bool survives(int neighbours) const noexcept
    {
        return neighbours >= 0 && neighbours <= 8 && (m_survival & (1u << neighbours)) != 0;
    }

    /// @brief Raw birth mask.
    ///
    /// @return Birth mask (bits 0..8).
    constexpr std::uint16_t birthMask() const noexcept
    {
        return m_birth;
    }

    /// @brief Raw survival mask.
    ///
    /// @return Survival mask (bits 0..8).
    constexpr std::uint16_t survivalMask() const noexcept
    {
        return m_survival;
    }

    /// @brief Render the canonical notation.
    ///
    /// @return Canonical notation, e.g. "B3/S23".
    std::string notation() const;

    /**
     * @brief Parse B/S notation.
     *
     * Accepts both "B3/S23" and the legacy "3/23" form, is case-insensitive
     * and ignores surrounding whitespace. Returns a typed error code; an empty
     * birth or survival set (e.g. "B2/S") is valid.
     * @param text Notation to parse.
     * @return Parsed rule or a typed error.
     */
    static RuleParseResult parse(std::string_view text);

    /// @brief Compare the two masks.
    ///
    /// @param lhs Left operand.
    /// @param rhs Right operand.
    /// @return True when both masks are equal.
    friend constexpr bool operator==(const Rule& lhs, const Rule& rhs) noexcept
    {
        return lhs.m_birth == rhs.m_birth && lhs.m_survival == rhs.m_survival;
    }

    /// @brief Compare the two masks.
    ///
    /// @param lhs Left operand.
    /// @param rhs Right operand.
    /// @return True when either mask differs.
    friend constexpr bool operator!=(const Rule& lhs, const Rule& rhs) noexcept
    {
        return !(lhs == rhs);
    }

private:
    std::uint16_t m_birth = static_cast<std::uint16_t>(1u << 3); ///< Birth mask; bit n = born at n.
    std::uint16_t m_survival = static_cast<std::uint16_t>((1u << 2) | (1u << 3)); ///< Survival mask.
};

/// @brief Result of Rule::parse().
struct RuleParseResult
{
    RuleParseError error = RuleParseError::None; ///< Parse error, or None.
    Rule rule;                                   ///< Parsed rule on success.
};

/// @brief A built-in rule with a stable, non-localized identifier.
struct RulePreset
{
    std::string_view id; ///< Stable preset ID.
    Rule rule;           ///< Rule value.
};

/// @brief Built-in rules in menu order.
///
/// @return Built-in rules in menu order.
const std::vector<RulePreset>& builtinRulePresets();

/// @brief Find a built-in rule by its stable ID.
///
/// @param id Stable preset ID.
/// @return Matching preset, or nullptr.
const RulePreset* findRulePreset(std::string_view id);

/// @brief Find a built-in rule by its canonical notation.
///
/// @param notation Canonical B/S notation.
/// @return Matching preset, or nullptr.
const RulePreset* findRulePresetByNotation(std::string_view notation);

} // namespace lifegame::domain
