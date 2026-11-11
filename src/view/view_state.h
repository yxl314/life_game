#pragma once

/**
 * @file view_state.h
 * @brief Atomic board frame, panel state and visible range values.
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
 * @brief One complete board frame.
 *
 * The presenter builds one frame per update and applies it atomically;
 * BoardView has no partial public setters for board/population/running.
 */
struct BoardFrame
{
    const domain::Grid* board = nullptr; ///< Read-only board (owned by the model).
    int population = 0;                  ///< Alive cells in this frame.
    bool running = false;                ///< Simulation running state.
};

/// @brief Half-open visible cell range used by the painter.
struct VisibleCellRange
{
    int firstRow = 0; ///< First visible row (inclusive).
    int lastRow = 0;  ///< Last visible row (inclusive).
    int firstCol = 0; ///< First visible column (inclusive).
    int lastCol = 0;  ///< Last visible column (inclusive).
};

/// @brief One complete side-panel state.
struct PanelState
{
    shared::LifeStats stats;                       ///< Generation/population/density.
    int width = 0;                                 ///< Board width.
    int height = 0;                                ///< Board height.
    QString ruleNotation = QStringLiteral("B3/S23"); ///< Canonical rule.
    domain::Boundary boundary = domain::Boundary::Dead; ///< Boundary policy.
    int oldestGeneration = 0;                      ///< Oldest rewind generation.
    bool running = false;                          ///< Simulation running state.
};

} // namespace lifegame::view
