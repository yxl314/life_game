#pragma once

/**
 * @file view_state.h
 * @brief 一次性应用的棋盘帧、面板状态与可见范围值。
 */

#include <QString>

#include "domain/boundary.h"
#include "shared/life_stats.h"

namespace lifegame::domain
{
class Grid;
}

namespace lifegame::view
{

/**
 * @brief 一整帧棋盘状态。
 *
 * 呈现器每次更新构造一帧并一次性应用；BoardView 没有分拆的
 * 棋盘/存活数/运行状态公开 setter。
 */
struct BoardFrame
{
    const domain::Grid* board = nullptr; ///< 只读棋盘（由模型持有）。
    int population = 0;                  ///< 本帧的存活细胞数。
    bool running = false;                ///< 模拟运行状态。
};

/// @brief 绘制器使用的可见格子范围（闭区间）。
struct VisibleCellRange
{
    int firstRow = 0; ///< 第一个可见行（含）。
    int lastRow = 0;  ///< 最后一个可见行（含）。
    int firstCol = 0; ///< 第一个可见列（含）。
    int lastCol = 0;  ///< 最后一个可见列（含）。
};

/// @brief 一整帧侧边面板状态。
struct PanelState
{
    shared::LifeStats stats;                       ///< 代数/存活数/密度。
    int width = 0;                                 ///< 棋盘宽度。
    int height = 0;                                ///< 棋盘高度。
    QString ruleNotation = QStringLiteral("B3/S23"); ///< 规范规则记法。
    domain::Boundary boundary = domain::Boundary::Dead; ///< 边界策略。
    int oldestGeneration = 0;                      ///< 可回退的最早代数。
    bool running = false;                          ///< 模拟运行状态。
};

} // namespace lifegame::view
