#pragma once

/**
 * @file rule_catalog.h
 * @brief 规则 ID/记法到显示名称的展示目录。
 */

#include <QObject>
#include <QString>
#include <QStringList>

#include <string>
#include <string_view>
#include <vector>

namespace lifegame::view
{

/// @brief 一条规则预设的显示元数据。
struct RulePresetView
{
    std::string id;      ///< 稳定的领域 ID。
    QString name;        ///< 本地化名称。
    QString notation;    ///< 规范 B/S 记法。
};

/// @brief 展示目录：把规则 ID/记法映射为本地化名称。
class RuleCatalog : public QObject
{
    Q_OBJECT

public:
    /// @brief 构造目录并填入内置规则预设。
    /// @param parent QObject 父对象。
    explicit RuleCatalog(QObject* parent = nullptr);

    /// @brief 返回所有预设元数据。
    /// @return 预设列表。
    const std::vector<RulePresetView>& presets() const;

    /// @brief 返回按显示顺序排列的本地化预设名。
    /// @return 名称列表。
    QStringList names() const;

    /// @brief 返回指定下标的规范记法。
    /// @param index 预设下标。
    /// @return 规范记法；下标越界时为空。
    QString notationAt(int index) const;

    /// @brief 按规范记法查找本地化名称。
    /// @param notation 规范记法。
    /// @return 本地化名称；找不到时原样返回记法。
    QString nameForNotation(const QString& notation) const;

private:
    std::vector<RulePresetView> m_presets; ///< 内置规则预设的显示元数据。
};

} // namespace lifegame::view
