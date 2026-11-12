#pragma once

/**
 * @file life_stats.h
 * @brief 只读的种群统计。
 */

namespace lifegame::shared
{

/// @brief 视图展示的统计数据的不可变快照。
struct LifeStats
{
    int generation = 0;   ///< 当前代数。
    int population = 0;   ///< 活细胞数。
    double density = 0.0; ///< 活细胞百分比，范围 [0, 100]。
};

} // namespace lifegame::shared
