#pragma once

/**
 * @file history.h
 * @brief 用于回退的有界按位打包快照历史。
 */

#include <memory>
#include <optional>
#include <vector>

#include "domain/grid.h"
#include "domain/packed_grid.h"

namespace lifegame::domain
{

/**
 * @brief 用于实现回退的有界按位打包快照历史。
 *
 * 生命游戏是多对一的，无法由当前代反推出上一代。因此每隔一个间隔保留
 * 一份快照（外加原点锚点），回退时从“不晚于目标代”的最近快照向前重放。
 *
 * 纯 C++ 实现，不依赖 Qt。
 */
class History
{
public:
    /// @brief 保留的一代及其打包棋盘。
    struct Snapshot
    {
        int generation = 0; ///< 该快照对应的代数。
        std::shared_ptr<const PackedGrid> packedBoard; ///< 打包后的棋盘副本。
    };

    static constexpr int kMaxSnapshots = 64; ///< 最多保留的中间快照数。

    /**
     * @brief 用给定快照间隔创建历史。
     * @param interval 快照间隔（代），必须为正。
     */
    explicit History(int interval = 100);

    /**
     * @brief 丢弃全部内容并开启新纪元。
     * @param generation 纪元起点代数。
     * @param board 可选的锚点棋盘。
     * @return 锚点棋盘打包失败（内存不足）时返回 false。
     */
    bool reset(int generation, const std::shared_ptr<const Grid>& board);

    /**
     * @brief 当代数对齐且更新时记录一份快照。
     * @param generation `board` 对应的代数。
     * @param board 待打包的棋盘；为空时忽略。
     * @return 打包失败（内存不足）时返回 false。
     */
    bool record(int generation, const std::shared_ptr<const Grid>& board);

    /**
     * @brief 记录一份已打包好的快照。
     * @param generation `board` 对应的代数。
     * @param board 已打包的棋盘；为空时忽略。
     * @return 快照无效（内存不足导致打包失败）时返回 false。
     */
    bool recordPacked(int generation, std::shared_ptr<const PackedGrid> board);

    /**
     * @brief 判断 record() 是否会真的保存内容。
     * @param generation 待判定的代数。
     * @return 已初始化、代数对齐且更新时返回 true。
     */
    bool shouldRecord(int generation) const;

    /**
     * @brief 丢弃指定代数及之后的快照。
     * @param generation 第一个失效的代数。
     */
    void invalidateFrom(int generation);

    /// @brief 查询快照间隔。
    /// @return 快照间隔（代）。
    int interval() const;

    /// @brief 查询当前纪元的起点。
    /// @return 纪元起点代数。
    int epochGeneration() const;

    /**
     * @brief 改变快照间隔，不重置历史。
     * @param interval 新的正间隔。
     */
    void setInterval(int interval);

    /// @brief 查询仍保留的最早一代。
    /// @return 最早保留的代数；历史为空时返回 0。
    int oldestGeneration() const;

    /**
     * @brief 找出不晚于给定代的最近快照。
     * @param generation 请求的代数。
     * @return 快照副本；没有可用快照时返回 std::nullopt。
     */
    std::optional<Snapshot> snapshotAtOrBefore(int generation) const;

    /// @brief 统计保留的快照数。
    /// @return 保留的快照数（含锚点）。
    std::size_t size() const noexcept
    {
        return m_snapshots.size();
    }

    /**
     * @brief 某个保留快照的代数。
     * @param index 快照下标。
     * @return 代数；下标无效时返回 -1。
     */
    int generationAt(std::size_t index) const noexcept
    {
        return index < m_snapshots.size() ? m_snapshots[index].generation : -1;
    }

    /// @brief 判断历史里是否没有任何快照。
    /// @return 没有保留快照时返回 true。
    bool empty() const
    {
        return m_snapshots.empty();
    }

private:
    /// @brief 判断代数是否与纪元、间隔对齐。
    /// @param generation 待判定的代数。
    /// @return 该代应当有快照时返回 true。
    bool aligned(int generation) const;

    /// @brief 丢弃超出保留上限的最早中间快照。
    ///
    /// 下标 0 的锚点始终保留。
    void trim();

    int m_interval = 100;              ///< 快照间隔（代），恒为正。
    int m_epoch = 0;                   ///< 当前纪元的起点代数。
    std::vector<Snapshot> m_snapshots; ///< 保留的快照，最早在前；下标 0 为锚点。
};

} // namespace lifegame::domain
