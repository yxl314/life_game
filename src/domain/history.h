#pragma once

/**
 * @file history.h
 * @brief Bounded bit-packed snapshot history for rewind.
 */

#include <memory>
#include <optional>
#include <vector>

#include "domain/grid.h"
#include "domain/packed_grid.h"

namespace lifegame::domain
{

/**
 * @brief Bounded bit-packed snapshot history used to implement rewind.
 *
 * Life is many-to-one, so the previous generation cannot be derived from the
 * current one. We keep one snapshot per interval (plus an anchor at the
 * origin) and replay forward from the closest snapshot not newer than the
 * requested generation.
 *
 * This class is pure C++ and has no Qt dependency.
 */
class History
{
public:
    /// @brief One retained generation and its packed board.
    struct Snapshot
    {
        int generation = 0; ///< Generation represented by the snapshot.
        std::shared_ptr<const PackedGrid> packedBoard; ///< Packed board copy.
    };

    static constexpr int kMaxSnapshots = 64; ///< Maximum retained interior snapshots.

    /**
     * @brief Create history with a snapshot interval.
     * @param interval Snapshot interval in generations; must be positive.
     */
    explicit History(int interval = 100);

    /**
     * @brief Discard everything and start a new epoch.
     * @param generation Epoch generation.
     * @param board Optional anchor board.
     * @return False when packing the anchor board failed (OOM).
     */
    bool reset(int generation, const std::shared_ptr<const Grid>& board);

    /**
     * @brief Record a snapshot when the generation is aligned and newer.
     * @param generation Generation represented by `board`.
     * @param board Board to pack; ignored when null.
     * @return False when packing failed (OOM).
     */
    bool record(int generation, const std::shared_ptr<const Grid>& board);

    /**
     * @brief Record an already packed snapshot.
     * @param generation Generation represented by `board`.
     * @param board Packed board; ignored when null.
     * @return False when the snapshot is invalid (OOM packing failed).
     */
    bool recordPacked(int generation, std::shared_ptr<const PackedGrid> board);

    /**
     * @brief Check whether record() would store something.
     * @param generation Generation to test.
     * @return True when aligned, newer, and history is initialized.
     */
    bool shouldRecord(int generation) const;

    /**
     * @brief Remove snapshots at or after a generation.
     * @param generation First generation to invalidate.
     */
    void invalidateFrom(int generation);

    /// @return Current snapshot interval in generations.
    int interval() const;

    /// @return Generation at which the current epoch started.
    int epochGeneration() const;

    /**
     * @brief Change the snapshot interval without resetting history.
     * @param interval New positive interval.
     */
    void setInterval(int interval);

    /// @return Oldest retained generation, or 0 when empty.
    int oldestGeneration() const;

    /**
     * @brief Find the closest snapshot not newer than a generation.
     * @param generation Requested generation.
     * @return Snapshot copy, or std::nullopt when none exists.
     */
    std::optional<Snapshot> snapshotAtOrBefore(int generation) const;

    /// @return Number of retained snapshots (including the anchor).
    std::size_t size() const noexcept
    {
        return m_snapshots.size();
    }

    /// Generation of a retained snapshot; returns -1 for an invalid index.
    /**
     * @brief Generation of a retained snapshot.
     * @param index Snapshot index.
     * @return Generation, or -1 for an invalid index.
     */
    int generationAt(std::size_t index) const noexcept
    {
        return index < m_snapshots.size() ? m_snapshots[index].generation : -1;
    }

    /// @return True when no snapshot is retained.
    bool empty() const
    {
        return m_snapshots.empty();
    }

private:
    bool aligned(int generation) const;
    void trim();

    int m_interval = 100;
    int m_epoch = 0;
    std::vector<Snapshot> m_snapshots;
};

} // namespace lifegame::domain
