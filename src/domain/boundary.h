#pragma once

/**
 * @file boundary.h
 * @brief 有限棋盘的边界策略。
 */

namespace lifegame::domain
{

/// @brief 棋盘外的细胞如何处理。
enum class Boundary
{
    Dead = 0, ///< 棋盘外一律视为死细胞。
    Wrap = 1  ///< 棋盘按环面环绕。
};

} // namespace lifegame::domain
