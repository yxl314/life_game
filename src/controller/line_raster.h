#pragma once

/**
 * @file line_raster.h
 * @brief Bresenham 直线栅格化辅助函数。
 */

#include <vector>

namespace lifegame::controller
{

/// @brief 整数格子坐标。
struct CellPosition
{
    int row = 0; ///< 行号。
    int col = 0; ///< 列号。
};

/// @brief 把两个格子之间的 Bresenham 直线（含两端）栅格化。
/// @param fromRow 起点行号。
/// @param fromCol 起点列号。
/// @param toRow 终点行号。
/// @param toCol 终点列号。
/// @return 沿直线排列的格子坐标，按从起点到终点的顺序。
std::vector<CellPosition> rasterLine(int fromRow, int fromCol, int toRow, int toCol);

} // namespace lifegame::controller
