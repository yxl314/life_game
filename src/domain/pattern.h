#pragma once

/**
 * @file pattern.h
 * @brief Immutable rectangular cell pattern.
 */

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "domain/cell.h"

namespace lifegame::domain
{

/**
 * @brief Immutable rectangular block of cells.
 *
 * Fields are private: callers use accessors and the constructor/factory. The
 * id is stable but carries no display text (names belong to the view).
 */
class Pattern
{
public:
    /// @brief Construct an empty pattern.
    Pattern() = default;

    /**
     * @brief Construct a validated pattern from explicit fields.
     * @param id Stable pattern identifier.
     * @param width Width in cells.
     * @param height Height in cells.
     * @param cells Row-major cells; size must equal width * height.
     * @return Pattern, or std::nullopt when the fields are inconsistent.
     */
    static std::optional<Pattern> fromCells(std::string id,
                                            int width,
                                            int height,
                                            std::vector<Cell> cells);

    /// @return Stable pattern identifier.
    const std::string& id() const noexcept
    {
        return m_id;
    }

    /// @return Pattern width in cells.
    int width() const noexcept
    {
        return m_width;
    }

    /// @return Pattern height in cells.
    int height() const noexcept
    {
        return m_height;
    }

    /// @return Row-major cell storage.
    const std::vector<Cell>& cells() const noexcept
    {
        return m_cells;
    }

    /**
     * @brief Read one pattern cell.
     * @param row Row index.
     * @param col Column index.
     * @return True when alive; false for dead or out-of-bounds.
     */
    bool alive(int row, int col) const;

    /// @return Number of alive cells.
    int population() const;

    /// @return True when width/height are non-negative and cells matches area.
    bool isConsistent() const;

    /**
     * @brief Build a pattern from ASCII art.
     *
     * 'O', 'o', '#', 'X' and 'x' are alive; every other character is dead.
     * @param id Stable pattern identifier.
     * @param rows ASCII rows; ragged rows are padded on the right.
     * @return Newly constructed pattern.
     */
    static Pattern fromAscii(std::string id, const std::vector<std::string>& rows);

private:
    Pattern(std::string id, int width, int height, std::vector<Cell> cells);

    std::string m_id;
    int m_width = 0;
    int m_height = 0;
    std::vector<Cell> m_cells;
};

} // namespace lifegame::domain
