#include "domain/rule.h"

#include <algorithm>
#include <string>

#include "domain/text_util.h"

namespace lifegame::domain
{

namespace
{

using detail::trim;

bool parseMask(std::string_view text, std::uint16_t* mask)
{
    std::uint16_t value = 0;
    for (const char ch : text)
    {
        if (ch < '0' || ch > '8')
        {
            return false;
        }
        value = static_cast<std::uint16_t>(value | (1u << (ch - '0')));
    }
    *mask = value;
    return true;
}

std::string maskToText(std::uint16_t mask)
{
    std::string text;
    for (int n = 0; n <= 8; ++n)
    {
        if ((mask & (1u << n)) != 0)
        {
            text.push_back(static_cast<char>('0' + n));
        }
    }
    return text;
}

std::string_view stripPrefix(std::string_view text, char upper, char lower)
{
    if (!text.empty() && (text.front() == upper || text.front() == lower))
    {
        text.remove_prefix(1);
    }
    return text;
}

} // namespace

std::string Rule::notation() const
{
    return "B" + maskToText(m_birth) + "/S" + maskToText(m_survival);
}

RuleParseResult Rule::parse(std::string_view text)
{
    RuleParseResult result;

    text = trim(text);
    if (text.empty())
    {
        result.error = RuleParseError::Empty;
        return result;
    }

    const std::size_t slash = text.find('/');
    if (slash == std::string_view::npos)
    {
        result.error = RuleParseError::MissingSeparator;
        return result;
    }

    std::string_view birthPart = trim(text.substr(0, slash));
    std::string_view survivalPart = trim(text.substr(slash + 1));

    birthPart = stripPrefix(birthPart, 'B', 'b');
    survivalPart = stripPrefix(survivalPart, 'S', 's');

    std::uint16_t birth = 0;
    std::uint16_t survival = 0;
    if (!parseMask(birthPart, &birth) || !parseMask(survivalPart, &survival))
    {
        result.error = RuleParseError::InvalidCharacter;
        return result;
    }

    result.rule = Rule(birth, survival);
    return result;
}

const std::vector<RulePreset>& builtinRulePresets()
{
    static const std::vector<RulePreset> presets = {
        {"conway", Rule()},
        {"highlife", Rule(static_cast<std::uint16_t>((1u << 3) | (1u << 6)),
                          static_cast<std::uint16_t>((1u << 2) | (1u << 3)))},
        {"daynight", Rule(static_cast<std::uint16_t>((1u << 3) | (1u << 6) | (1u << 7) | (1u << 8)),
                          static_cast<std::uint16_t>((1u << 3) | (1u << 4) | (1u << 6) | (1u << 7) | (1u << 8)))},
        {"seeds", Rule(static_cast<std::uint16_t>(1u << 2), 0)},
        {"maze", Rule(static_cast<std::uint16_t>(1u << 3),
                      static_cast<std::uint16_t>((1u << 1) | (1u << 2) | (1u << 3) | (1u << 4) | (1u << 5)))},
    };
    return presets;
}

const RulePreset* findRulePreset(std::string_view id)
{
    const auto& presets = builtinRulePresets();
    const auto it = std::find_if(presets.begin(), presets.end(), [id](const RulePreset& preset) {
        return preset.id == id;
    });
    return it == presets.end() ? nullptr : &*it;
}

const RulePreset* findRulePresetByNotation(std::string_view notation)
{
    const auto& presets = builtinRulePresets();
    const auto it = std::find_if(presets.begin(), presets.end(), [notation](const RulePreset& preset) {
        return preset.rule.notation() == notation;
    });
    return it == presets.end() ? nullptr : &*it;
}

} // namespace lifegame::domain
