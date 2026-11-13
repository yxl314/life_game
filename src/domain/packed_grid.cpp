#include "domain/packed_grid.h"

#include "domain/grid.h"

#include <algorithm>
#include <cstring>
#include <new>

namespace lifegame::domain
{

namespace
{

std::unique_ptr<std::uint64_t[]> allocateWords(std::size_t count)
{
    if (count == 0)
    {
        return {};
    }
    return std::unique_ptr<std::uint64_t[]>(new (std::nothrow) std::uint64_t[count]);
}

} // namespace

PackedGrid::PackedGrid(const Grid& board)
    : m_width(board.width()), m_height(board.height())
{
    if (m_width <= 0 || m_height <= 0)
    {
        m_valid = false;
        return;
    }
    const std::size_t count =
        static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    m_wordCount = (count + 63u) / 64u;
    m_words = allocateWords(m_wordCount);
    if (m_wordCount > 0 && !m_words)
    {
        m_valid = false;
        m_wordCount = 0;
        return;
    }
    if (m_wordCount > 0)
    {
        std::memset(m_words.get(), 0, m_wordCount * sizeof(std::uint64_t));
    }

    const Cell* cells = board.data();
    if (cells == nullptr)
    {
        m_valid = false;
        m_wordCount = 0;
        m_words.reset();
        return;
    }
    for (std::size_t i = 0; i < count; ++i)
    {
        if (cells[i] != kDead)
        {
            m_words[i >> 6] |= (std::uint64_t(1) << (i & 63u));
        }
    }
}

PackedGrid::PackedGrid(const PackedGrid& other)
    : m_width(other.m_width),
      m_height(other.m_height),
      m_wordCount(other.m_wordCount),
      m_words(allocateWords(other.m_wordCount)),
      m_valid(other.m_valid && (other.m_wordCount == 0 || m_words != nullptr))
{
    if (m_valid && m_wordCount > 0)
    {
        std::memcpy(m_words.get(), other.m_words.get(), m_wordCount * sizeof(std::uint64_t));
    }
}

PackedGrid& PackedGrid::operator=(const PackedGrid& other)
{
    if (this == &other)
    {
        return *this;
    }
    m_width = other.m_width;
    m_height = other.m_height;
    m_wordCount = other.m_wordCount;
    m_words = allocateWords(other.m_wordCount);
    m_valid = other.m_valid && (other.m_wordCount == 0 || m_words != nullptr);
    if (m_valid && m_wordCount > 0)
    {
        std::memcpy(m_words.get(), other.m_words.get(), m_wordCount * sizeof(std::uint64_t));
    }
    return *this;
}

Grid PackedGrid::unpack() const
{
    if (!m_valid)
    {
        return {};
    }

    Grid board(m_width, m_height);
    if (m_wordCount == 0)
    {
        return board;
    }

    const int width = m_width;
    const int height = m_height;
    for (int row = 0; row < height; ++row)
    {
        const std::size_t rowStart = static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        for (int col = 0; col < width; ++col)
        {
            const std::size_t index = rowStart + static_cast<std::size_t>(col);
            if ((m_words[index >> 6] & (std::uint64_t(1) << (index & 63u))) != 0)
            {
                board.setCell(row, col, true);
            }
        }
    }
    return board;
}

} // namespace lifegame::domain
