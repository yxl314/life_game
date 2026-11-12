#pragma once

/**
 * @file pattern_catalog.h
 * @brief 图案 ID 到显示名称的展示目录。
 */

#include <QObject>
#include <QString>

#include <string>
#include <string_view>
#include <vector>

#include "domain/pattern.h"

namespace lifegame::view
{

/// @brief 一个图案的显示元数据。
struct PatternInfo
{
    std::string id;      ///< 稳定的领域 ID。
    QString name;        ///< 本地化名称。
    QString category;    ///< 本地化分类。
    QString description; ///< 本地化说明。
};

/// @brief 展示目录：把图案 ID 映射为本地化文本。
class PatternCatalog : public QObject
{
    Q_OBJECT

public:
    /// @brief 构造目录并填入内置图案的显示文本。
    /// @param parent QObject 父对象。
    explicit PatternCatalog(QObject* parent = nullptr);

    /// @brief 返回所有图案元数据。
    /// @return 元数据列表。
    const std::vector<PatternInfo>& all() const;

    /// @brief 按 ID 查找图案元数据。
    /// @param id 稳定的图案 ID。
    /// @return 匹配的元数据；找不到时返回 nullptr。
    const PatternInfo* find(std::string_view id) const;

    /// @brief 生成图案的显示标签。
    /// @param pattern 领域图案。
    /// @return 本地化标签；未知图案返回导入图案的标签。
    QString displayLabel(const domain::Pattern& pattern) const;

    /// @brief 返回图案的本地化说明。
    /// @param id 稳定的图案 ID。
    /// @return 本地化说明；未知 ID 返回通用说明。
    QString descriptionOf(std::string_view id) const;

private:
    std::vector<PatternInfo> m_patterns; ///< 内置图案的显示元数据。
};

} // namespace lifegame::view
