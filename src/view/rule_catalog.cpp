#include "view/rule_catalog.h"

#include "domain/rule.h"

namespace lifegame::view
{

namespace
{

QString nameForId(std::string_view id)
{
    if (id == "conway")
    {
        return RuleCatalog::tr("康威生命");
    }
    if (id == "highlife")
    {
        return RuleCatalog::tr("高地生活");
    }
    if (id == "daynight")
    {
        return RuleCatalog::tr("昼与夜");
    }
    if (id == "seeds")
    {
        return RuleCatalog::tr("种子");
    }
    if (id == "maze")
    {
        return RuleCatalog::tr("迷宫");
    }
    return QString::fromStdString(std::string(id));
}

} // namespace

RuleCatalog::RuleCatalog(QObject* parent)
    : QObject(parent)
{
    for (const domain::RulePreset& preset : domain::builtinRulePresets())
    {
        m_presets.push_back({std::string(preset.id),
                             nameForId(preset.id),
                             QString::fromStdString(preset.rule.notation())});
    }
}

const std::vector<RulePresetView>& RuleCatalog::presets() const
{
    return m_presets;
}

QStringList RuleCatalog::names() const
{
    QStringList result;
    for (const RulePresetView& preset : m_presets)
    {
        result.append(preset.name);
    }
    return result;
}

QString RuleCatalog::notationAt(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_presets.size()))
    {
        return {};
    }
    return m_presets[static_cast<std::size_t>(index)].notation;
}

QString RuleCatalog::nameForNotation(const QString& notation) const
{
    for (const RulePresetView& preset : m_presets)
    {
        if (preset.notation == notation)
        {
            return preset.name;
        }
    }
    return notation;
}

} // namespace lifegame::view
