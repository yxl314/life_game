#pragma once

/**
 * @file grid.h
 * @brief 棋盘存储与按行并行的生命演化。
 */

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "domain/board_limits.h"
#include "domain/boundary.h"
#include "domain/cell.h"
#include "domain/rule.h"

namespace lifegame::domain
{

/// @brief 计算一代时产出的统计量。
struct GridStats
{
    int population = 0;   ///< 计算范围内的活细胞数。
    bool changed = false; ///< 是否有任一格子与源状态不同。
};

/**
 * @brief 堆上存储、活动细胞集中在一个连续向量里的棋盘。
 *
 * 存储只按当前 width() * height() 的活动区域分配，小棋盘不会为 150 万格
 * 付费；尺寸不变时向量由演化流程复用；Grid 对象本身很小，可以放在栈上，
 * 这对 1500x1000 的棋盘很重要。
 */
class Grid
{
public:
    static constexpr int kMaxWidth = kMaxBoardWidth;   ///< 支持的最大宽度。
    static constexpr int kMaxHeight = kMaxBoardHeight; ///< 支持的最大高度。
    static constexpr int kMaxCells = kMaxWidth * kMaxHeight; ///< 支持的最大格数。

    /// @brief 构造一个空的（零尺寸）棋盘。
    Grid();

    /**
     * @brief 构造棋盘并分配其活动存储。
     * @param width 请求的宽度（钳制到 [0, kMaxWidth]）。
     * @param height 请求的高度（钳制到 [0, kMaxHeight]）。
     */
    explicit Grid(int width, int height);

    /**
     * @brief 调整活动区域。
     * @param width 新宽度。
     * @param height 新高度。
     *
     * 尺寸变化时按新活动区域重新分配并清零；尺寸与存储都没变时是空操作，
     * 这样每格都会重写的演化流程可以省掉每次一代的大块 memset。
     */
    void resize(int width, int height);

    /// @brief 清空所有活动细胞，不改变尺寸。
    void clear();

    /// @brief 查询活动宽度。
    /// @return 活动宽度（格）。
    constexpr int width() const noexcept
    {
        return m_width;
    }

    /// @brief 查询活动高度。
    /// @return 活动高度（格）。
    constexpr int height() const noexcept
    {
        return m_height;
    }

    /// @brief 判断坐标是否落在活动区域内。
    /// @param row 行号。
    /// @param col 列号。
    /// @return 坐标在活动区域内时返回 true。
    bool inBounds(int row, int col) const;

    /// @brief 读取一个细胞。
    /// @param row 行号。
    /// @param col 列号。
    /// @return 细胞存活时返回 true；死细胞或越界返回 false。
    bool cell(int row, int col) const;

    /**
     * @brief 设置一个细胞。
     * @param row 行号。
     * @param col 列号。
     * @param alive 新状态。
     * @note 越界坐标被忽略。
     */
    void setCell(int row, int col, bool alive);

    /**
     * @brief 取反一个界内细胞。
     * @param row 行号。
     * @param col 列号。
     * @return 新状态；越界时返回 std::nullopt。
     */
    std::optional<bool> toggleCell(int row, int col);

    /// @brief 统计活动区域的活细胞数。
    ///
    /// 需要扫描整个活动存储，代价与 width() * height() 成正比。
    /// @return 活动区域的活细胞数。
    int population() const;

    /**
     * @brief 统计一个格子的八个邻居。
     * @param row 行号。
     * @param col 列号。
     * @param boundary 边界策略。
     * @return [0, 8] 内的邻居数。
     */
    int neighbourCount(int row, int col, Boundary boundary) const;

    /**
     * @brief 原地串行演化一代。
     * @param rule 生命类规则。
     * @param boundary 边界策略。
     * @note 主要用于测试与单步；异步流程走 computeNext()/computeRows()。
     */
    void advance(const Rule& rule, Boundary boundary);

    /**
     * @brief 按密度随机填充棋盘。
     * @param densityPercent 活细胞百分比（[0, 100]）。
     * @param seed 确定性的随机种子。
     */
    void randomize(int densityPercent, std::uint32_t seed);

    /// @brief 比较尺寸与每个活动细胞。
    /// @param other 待比较的棋盘。
    /// @return 尺寸与所有活动细胞都相同时返回 true。
    bool equals(const Grid& other) const;

    /// @brief 查询活动存储的字节数。
    /// @return 当前为活动细胞分配的字节数。
    std::size_t activeCellStorage() const noexcept
    {
        return m_cells.size();
    }

    /// @brief 访问活动存储。
    /// @return 指向活动存储的只读指针（非空棋盘上不会为空）。
    const Cell* data() const
    {
        return m_cells.data();
    }

    /**
     * @brief 串行地把整代结果算进 `next`。
     *
     * @param current 源代，不能与 `next` 别名。
     * @param next 输出代，会调整到 `current` 的尺寸。
     * @param rule 生命类规则。
     * @param boundary 边界策略。
     * @param stats 可选的输出统计，计算前会被重置。
     * @note `next` 不能与 `current` 是同一个对象。
     */
    static void computeNext(const Grid& current,
                            Grid* next,
                            const Rule& rule,
                            Boundary boundary,
                            GridStats* stats = nullptr);

    /**
     * @brief 把半开行区间 [rowBegin, rowEnd) 算进 `next`。
     *
     * @param current 只读的源代。
     * @param next 输出代，尺寸必须与源代相同。
     * @param rule 生命类规则。
     * @param boundary 边界策略。
     * @param rowBegin 起始行（含）。
     * @param rowEnd 结束行（不含）。
     * @param stats 可选的区间统计，计算前会被重置。
     * @note 不同区间只读 `current` 并写 `next` 中互不相交的行，可以并发执行。
     */
    static void computeRows(const Grid& current,
                            Grid* next,
                            const Rule& rule,
                            Boundary boundary,
                            int rowBegin,
                            int rowEnd,
                            GridStats* stats = nullptr);

private:
    int m_width = 0;           ///< 活动宽度。
    int m_height = 0;          ///< 活动高度。
    std::vector<Cell> m_cells; ///< 活动区域的行主序细胞。
};

} // namespace lifegame::domain
