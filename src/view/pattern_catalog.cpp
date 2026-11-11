#include "view/pattern_catalog.h"

#include <algorithm>

namespace lifegame::view
{

PatternCatalog::PatternCatalog(QObject* parent)
    : QObject(parent),
      m_patterns{
        {"glider", tr("滑翔机"), tr("飞船"), tr("最小的飞船，每 4 代沿对角线移动一格。")},
        {"gosper_gun", tr("高斯帕滑翔机炮"), tr("增长"),
         tr("1969 年发现，每 30 代向右发射一只滑翔机。")},
        {"pulsar", tr("脉冲星"), tr("振荡器"), tr("经典周期 3 振荡器，由 48 个细胞组成。")},
        {"blinker", tr("闪烁灯"), tr("振荡器"), tr("最简单的振荡器，周期 2。")},
        {"block", tr("方块"), tr("静物"), tr("2×2 的静物，永不变化。")},
        {"beehive", tr("蜂巢"), tr("静物"), tr("六格静物，形状像蜂巢。")},
        {"toad", tr("蟾蜍"), tr("振荡器"), tr("周期 2 振荡器，形状像游动的蟾蜍。")},
        {"beacon", tr("信标"), tr("振荡器"), tr("周期 2 振荡器，由两个方块对角组成。")},
        {"pentadecathlon", tr("五连珠"), tr("振荡器"), tr("周期 15 振荡器，又名十项全能。")},
        {"lwss", tr("轻型飞船"), tr("飞船"), tr("每 4 代水平移动两格。")},
        {"acorn", tr("橡果"), tr("长寿"), tr("初始仅 7 个细胞，演化超过 5000 代后才稳定。")},
        {"r_pentomino", tr("R 五连体"), tr("长寿"), tr("5 个细胞引发长时间混沌，是生命游戏的经典样例。")},
        {"diehard", tr("死亡之翼"), tr("长寿"), tr("仅 7 个细胞，却要经过 130 代才彻底消失。")},
      }
{
}

const std::vector<PatternInfo>& PatternCatalog::all() const
{
    return m_patterns;
}

const PatternInfo* PatternCatalog::find(std::string_view id) const
{
    const auto it = std::find_if(m_patterns.begin(), m_patterns.end(), [id](const PatternInfo& info) {
        return info.id == id;
    });
    return it == m_patterns.end() ? nullptr : &*it;
}

QString PatternCatalog::displayLabel(const domain::Pattern& pattern) const
{
    if (const PatternInfo* info = find(pattern.id()))
    {
        return QStringLiteral("%1 · %2").arg(info->category, info->name);
    }
    return tr("导入 · RLE 图案");
}

QString PatternCatalog::descriptionOf(std::string_view id) const
{
    if (const PatternInfo* info = find(id))
    {
        return info->description;
    }
    return tr("从 RLE 文本载入的图案。");
}

} // namespace lifegame::view
