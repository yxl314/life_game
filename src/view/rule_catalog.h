#pragma once

/**
 * @file rule_catalog.h
 * @brief Rule ID to display-name presentation catalog.
 */

#include <QObject>
#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace lifegame::view
{

/// @brief Display metadata for one rule preset.
struct RulePresetView
{
    std::string id;      ///< Stable domain ID.
    QString name;        ///< Localized name.
    QString notation;    ///< Canonical B/S notation.
};

/// @brief Presentation catalog: maps rule IDs/notations to localized names.
class RuleCatalog : public QObject
{
    Q_OBJECT

public:
    explicit RuleCatalog(QObject* parent = nullptr);

    /// @return All preset metadata.
    const std::vector<RulePresetView>& presets() const;

    /// @return Localized preset names in display order.
    QStringList names() const;

    /// @param index Preset index.
    /// @return Canonical notation, or empty.
    QString notationAt(int index) const;

    /// @param notation Canonical notation.
    /// @return Localized name, or the notation when unknown.
    QString nameForNotation(const QString& notation) const;

private:
    std::vector<RulePresetView> m_presets;
};

} // namespace lifegame::view
