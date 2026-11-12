#pragma once

/**
 * @file packed_grid.h
 * @brief 每格一比特的只读棋盘快照。
 */

#include <cstddef>
#include <cstdint>
#include <memory>

namespace lifegame::domain
{

class Grid;

/**
 * @brief 每格一比特的只读棋盘快照。
 *
 * 只有历史快照使用这种形式：活动棋盘与演化流程保持每格一字节，以便邻居
 * 读取命中缓存。打包每份快照只做一次，解包只在回退开始时发生。
 *
 * 存储用 `new (std::nothrow)` 分配，快照失败可以按 `OutOfMemory` 上报，
 * 而不是终止进程。
 */
class PackedGrid
{
public:
    /// @brief 构造一个空的、有效的快照。
    PackedGrid() = default;
    /// @brief 深拷贝打包字；源快照无效时副本也无效。
    /// @param other 待拷贝的快照。
    PackedGrid(const PackedGrid& other);
    /// @brief 深拷贝打包字；自赋值保留原有存储。
    /// @param other 待拷贝的快照。
    /// @return 本快照。
    PackedGrid& operator=(const PackedGrid& other);
    /// @brief 接管 `other` 的打包字。
    /// @param other 待移动的快照。
    PackedGrid(PackedGrid&&) noexcept = default;
    /// @brief 接管 `other` 的打包字。
    /// @param other 待移动的快照。
    /// @return 本快照。
    PackedGrid& operator=(PackedGrid&&) noexcept = default;

    /**
     * @brief 打包一块活动棋盘。
     * @param board 待复制成打包形式的棋盘。
     */
    explicit PackedGrid(const Grid& board);

    /// @brief 把快照解包成活动棋盘。
    /// @return 解包后的棋盘；快照无效时为空棋盘。
    Grid unpack() const;

    /// @brief 查询棋盘宽度。
    /// @return 宽度（格）。
    int width() const noexcept
    {
        return m_width;
    }

    /// @brief 查询棋盘高度。
    /// @return 高度（格）。
    int height() const noexcept
    {
        return m_height;
    }

    /// @brief 查询表示的比特数。
    /// @return 比特数（width * height）。
    std::size_t bitSize() const noexcept
    {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    /// @brief 查询打包存储的大小。
    /// @return 打包字占用的字节数。
    std::size_t byteSize() const noexcept
    {
        return m_wordCount *
               sizeof(std::uint64_t);
    }

    /// @brief 判断是否没有存储任何内容。
    /// @return 没有任何打包字时返回 true。
    bool empty() const noexcept
    {
        return m_wordCount == 0;
    }

    /// @brief 判断快照是否可用。
    /// @return 存储分配与打包都成功时返回 true。
    bool valid() const noexcept
    {
        return m_valid;
    }

private:
    int m_width = 0;                          ///< 棋盘宽度（格）。
    int m_height = 0;                         ///< 棋盘高度（格）。
    std::size_t m_wordCount = 0;              ///< 64 位字数；为空或失败时为 0。
    std::unique_ptr<std::uint64_t[]> m_words; ///< 打包字；为空或失败时为空指针。
    bool m_valid = true;                      ///< 分配或打包失败时为 false。
};

} // namespace lifegame::domain
