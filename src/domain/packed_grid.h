#pragma once

/**
 * @file packed_grid.h
 * @brief One-bit-per-cell immutable board snapshot.
 */

#include <cstddef>
#include <cstdint>
#include <memory>

namespace lifegame::domain
{

class Grid;

/**
 * @brief One-bit-per-cell immutable snapshot of a board.
 *
 * Only history snapshots use this form: the live board and the simulation
 * pipeline stay byte-per-cell for cache-friendly neighbour reads. Packing is
 * done once per snapshot; unpacking happens only when a rewind starts.
 *
 * Storage is allocated with `new (std::nothrow)` so snapshot failures can be
 * reported as `OutOfMemory` instead of terminating the process.
 */
class PackedGrid
{
public:
    /// @brief Construct an empty, valid snapshot.
    PackedGrid() = default;
    PackedGrid(const PackedGrid& other);
    PackedGrid& operator=(const PackedGrid& other);
    PackedGrid(PackedGrid&&) noexcept = default;
    PackedGrid& operator=(PackedGrid&&) noexcept = default;

    /**
     * @brief Pack a live board.
     * @param board Board to copy into packed form.
     */
    explicit PackedGrid(const Grid& board);

    /// @return Unpacked live board; empty when the snapshot is invalid.
    Grid unpack() const;

    /// @return Board width in cells.
    int width() const noexcept
    {
        return m_width;
    }

    /// @return Board height in cells.
    int height() const noexcept
    {
        return m_height;
    }

    /// @return Number of represented bits (width * height).
    std::size_t bitSize() const noexcept
    {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    /// @return Bytes used by the packed word storage.
    std::size_t byteSize() const noexcept
    {
        return m_wordCount * sizeof(std::uint64_t);
    }

    /// @return True when no packed words are stored.
    bool empty() const noexcept
    {
        return m_wordCount == 0;
    }

    /// @return True when storage allocation and packing succeeded.
    bool valid() const noexcept
    {
        return m_valid;
    }

private:
    int m_width = 0;
    int m_height = 0;
    std::size_t m_wordCount = 0;
    std::unique_ptr<std::uint64_t[]> m_words;
    bool m_valid = true;
};

} // namespace lifegame::domain
