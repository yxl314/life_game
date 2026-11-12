#pragma once

#include <cstdint>
#include <vector>

/**
 * @file cell.h
 * @brief 细胞存储类型与存活/死亡取值。
 */

namespace lifegame::domain
{

/**
 * @brief 紧凑的细胞存储：每个细胞一字节。
 *
 * 每格一字节让活动棋盘对缓存友好；历史快照改用 PackedGrid 按位打包。
 */
using Cell = std::uint8_t;

inline constexpr Cell kDead = 0; ///< 死细胞取值。
inline constexpr Cell kAlive = 1; ///< 活细胞取值。

/**
 * @brief 从行主序缓冲区读取一个细胞。
 *
 * Grid 与 Pattern 用同样的一字节行主序布局，都需要先做范围检查再索引。
 * 把公式集中在一处，布局变化时不会有一方越界访问，范围检查的改动也不会
 * 只落到其中一方。
 *
 * @param cells 细胞缓冲区，其大小决定有效范围。
 * @param width 每行的格数。
 * @param row 行号，可以越界。
 * @param col 列号，可以越界。
 * @return 坐标在范围内且该格存活时返回 true。
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
