#include "domain/pattern.h"

#include <algorithm>
#include <utility>

namespace lifegame::domain
{

Pattern::Pattern(std::string id, int width, int height, std::vector<Cell> cells)
    : m_id(std::move(id)), m_width(width), m_height(height), m_cells(std::move(cells))
{
}

std::optional<Pattern> Pattern::fromCells(std::string id,
                                          int width,
                                          int height,
                                          std::vector<Cell> cells)
{
    if (width < 0 || height < 0 ||
        cells.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height))
    {
        return std::nullopt;
    }
    return Pattern(std::move(id), width, height, std::move(cells));
}

bool Pattern::alive(int row, int col) const
{
    return cell_at(m_cells, m_width, row, col);
}

bool Pattern::isConsistent() const
{
    if (m_width < 0 || m_height < 0)
    {
        return false;
    }
    return m_cells.size() == static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
}

int Pattern::population() const
{
    int count = 0;
    for (const Cell value : m_cells)
    {
        count += value != kDead ? 1 : 0;
    }
    return count;
}

Pattern Pattern::fromAscii(std::string id, const std::vector<std::string>& rows)
{
    const int height = static_cast<int>(rows.size());
    std::size_t widest = 0;
    for (const std::string& row : rows)
    {
        widest = std::max(widest, row.size());
    }
    const int width = static_cast<int>(widest);

    std::vector<Cell> cells(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), kDead);
    for (int row = 0; row < height; ++row)
    {
        const std::string& text = rows[static_cast<std::size_t>(row)];
        for (std::size_t col = 0; col < text.size(); ++col)
        {
            const char ch = text[col];
            const bool alive = ch == 'O' || ch == 'o' || ch == '#' || ch == 'X' || ch == 'x';
            cells[static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + col] =
                alive ? kAlive : kDead;
        }
    }
    return Pattern(std::move(id), width, height, std::move(cells));
}

} // namespace lifegame::domain
