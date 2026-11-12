#pragma once

#include <cstdint>
#include <vector>

/**
 * @file cell.h
 * @brief Cell storage type and alive/dead values.
 */

namespace lifegame::domain
{

/**
 * @brief Compact cell storage: one byte per cell.
 *
 * One byte keeps the live board cache-friendly; packed history snapshots use
 * PackedGrid instead.
 */
using Cell = std::uint8_t;

inline constexpr Cell kDead = 0; ///< Dead cell value.
inline constexpr Cell kAlive = 1; ///< Alive cell value.

/**
 * @brief Read a cell from a row-major buffer.
 *
 * Grid and Pattern store cells the same way — one byte per cell, row-major —
 * and both need the same bounds check before indexing. Keeping the formula in
 * one place means a layout change cannot leave one of them indexing out of
 * bounds, and a fix to the range check cannot be applied to only one of them.
 *
 * @param cells Cell buffer; its size defines the valid extent.
 * @param width Row width in cells.
 * @param row Row index; may be out of range.
 * @param col Column index; may be out of range.
 * @return True when the position is in range and alive.
 */
inline bool cell_at(const std::vector<Cell>& cells, int width, int row, int col)
{
    if (row < 0 || col < 0 || col >= width)
    {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(row) * static_cast<std::size_t>(width) +
                              static_cast<std::size_t>(col);
    return index < cells.size() && cells[index] != kDead;
}

} // namespace lifegame::domain
