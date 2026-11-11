#pragma once

/**
 * @file grid.h
 * @brief Board storage and row-parallel Life evolution.
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

/// @brief Aggregate statistics produced while computing a generation.
struct GridStats
{
    int population = 0; ///< Alive cells in the computed range/board.
    bool changed = false; ///< True when any computed cell differs from its source.
};

/**
 * @brief Heap-backed board with one contiguous active-cell vector.
 *
 * Storage is sized to the active width() * height() area only, so a small
 * board does not pay for a 1.5M-cell buffer. The vector is reused by the step
 * pipeline when dimensions are unchanged, and the Grid object itself stays
 * small enough to live on the stack. This is important for 1500x1000 boards.
 */
class Grid
{
public:
    static constexpr int kMaxWidth = kMaxBoardWidth;   ///< Maximum supported width.
    static constexpr int kMaxHeight = kMaxBoardHeight; ///< Maximum supported height.
    static constexpr int kMaxCells = kMaxWidth * kMaxHeight; ///< Maximum supported cells.

    /// @brief Construct an empty (zero-sized) board.
    Grid();

    /**
     * @brief Construct a board and allocate its active storage.
     * @param width Requested width (clamped to [0, kMaxWidth]).
     * @param height Requested height (clamped to [0, kMaxHeight]).
     */
    explicit Grid(int width, int height);

    /**
     * @brief Resize the active area.
     * @param width New width.
     * @param height New height.
     *
     * When dimensions change the storage is sized to the new active area and
     * cleared. When dimensions and storage are unchanged this is a no-op:
     * callers that overwrite every active cell (the step pipeline) avoid a
     * large memset per generation.
     */
    void resize(int width, int height);

    /// @brief Clear all active cells without changing the dimensions.
    void clear();

    /// @return Active width in cells.
    constexpr int width() const noexcept
    {
        return m_width;
    }

    /// @return Active height in cells.
    constexpr int height() const noexcept
    {
        return m_height;
    }

    /// @param row Row index.
    /// @param col Column index.
    /// @return True when the coordinate is inside the active area.
    bool inBounds(int row, int col) const;

    /// @param row Row index.
    /// @param col Column index.
    /// @return True when the cell is alive; false for dead or out-of-bounds.
    bool cell(int row, int col) const;

    /**
     * @brief Set one cell.
     * @param row Row index.
     * @param col Column index.
     * @param alive New state.
     * @note Out-of-bounds coordinates are ignored.
     */
    void setCell(int row, int col, bool alive);

    /**
     * @brief Toggle an in-bounds cell.
     * @param row Row index.
     * @param col Column index.
     * @return New state, or std::nullopt when out of bounds.
     */
    std::optional<bool> toggleCell(int row, int col);

    /// @return Number of alive cells in the active area.
    int population() const;

    /**
     * @brief Count the eight neighbours of one cell.
     * @param row Row index.
     * @param col Column index.
     * @param boundary Boundary policy.
     * @return Neighbour count in [0, 8].
     */
    int neighbourCount(int row, int col, Boundary boundary) const;

    /**
     * @brief Evolve one generation in place (serial).
     * @param rule Life-like rule.
     * @param boundary Boundary policy.
     * @note Primarily for tests and single-step; the async pipeline uses
     *       computeNext()/computeRows().
     */
    void advance(const Rule& rule, Boundary boundary);

    /**
     * @brief Fill the board randomly.
     * @param densityPercent Alive percentage in [0, 100].
     * @param seed Deterministic RNG seed.
     */
    void randomize(int densityPercent, std::uint32_t seed);

    /// @param other Board to compare.
    /// @return True when dimensions and every active cell are equal.
    bool equals(const Grid& other) const;

    /// @return Number of bytes currently allocated for active cells.
    std::size_t activeCellStorage() const noexcept
    {
        return m_cells.size();
    }

    /// @return Read-only pointer to the active storage (never null for non-empty).
    const Cell* data() const
    {
        return m_cells.data();
    }

    /**
     * @brief Compute a whole generation into `next` (serial).
     *
     * @param current Source generation; must not alias `next`.
     * @param next Output generation; resized to `current` dimensions.
     * @param rule Life-like rule.
     * @param boundary Boundary policy.
     * @param stats Optional output statistics; reset before computing.
     * @note `next` must not alias `current`.
     */
    static void computeNext(const Grid& current,
                            Grid* next,
                            const Rule& rule,
                            Boundary boundary,
                            GridStats* stats = nullptr);

    /**
     * @brief Compute a half-open row range into `next`.
     *
     * @param current Read-only source generation.
     * @param next Output generation; must have the same dimensions.
     * @param rule Life-like rule.
     * @param boundary Boundary policy.
     * @param rowBegin First row (inclusive).
     * @param rowEnd One-past-last row (exclusive).
     * @param stats Optional output statistics for this range; reset first.
     * @note Different ranges only read `current` and write disjoint rows of
     *       `next`, so they may run concurrently.
     */
    static void computeRows(const Grid& current,
                            Grid* next,
                            const Rule& rule,
                            Boundary boundary,
                            int rowBegin,
                            int rowEnd,
                            GridStats* stats = nullptr);

private:
    int m_width = 0;
    int m_height = 0;
    std::vector<Cell> m_cells;
};

} // namespace lifegame::domain
