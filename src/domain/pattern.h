#pragma once

/**
 * @file pattern.h
 * @brief 不可变的矩形细胞图案。
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
 * @brief 不可变的矩形细胞块。
 *
 * 字段是私有的：调用方通过访问器与构造/工厂函数使用。ID 稳定但不含显示
 * 文本（名称属于视图层）。
 */
class Pattern
{
public:
    /// @brief 构造一个空图案。
    Pattern() = default;

    /**
     * @brief 用显式字段构造一个校验过的图案。
     * @param id 稳定的图案标识。
     * @param width 宽度（格）。
     * @param height 高度（格）。
     * @param cells 行主序细胞；大小必须等于 width * height。
     * @return 图案；字段不一致时返回 std::nullopt。
     */
    static std::optional<Pattern> fromCells(std::string id,
                                            int width,
                                            int height,
                                            std::vector<Cell> cells);

    /// @brief 查询稳定标识。
    /// @return 稳定的图案标识。
    const std::string& id() const noexcept
    {
        return m_id;
    }

    /// @brief 查询图案宽度。
    /// @return 宽度（格）。
    int width() const noexcept
    {
        return m_width;
    }

    /// @brief 查询图案高度。
    /// @return 高度（格）。
    int height() const noexcept
    {
        return m_height;
    }

    /// @brief 访问行主序细胞存储。
    /// @return 行主序细胞存储。
    const std::vector<Cell>& cells() const noexcept
    {
        return m_cells;
    }

    /**
     * @brief 读取图案中的一个细胞。
     * @param row 行号。
     * @param col 列号。
     * @return 细胞存活时返回 true；死细胞或越界返回 false。
     */
    bool alive(int row, int col) const;

    /// @brief 统计活细胞数。
    /// @return 活细胞数。
    int population() const;

    /// @brief 检查存储字段是否自洽。
    /// @return 宽高非负且 cells 大小等于面积时返回 true。
    bool isConsistent() const;

    /**
     * @brief 用 ASCII 图案构造图案。
     *
     * 'O'、'o'、'#'、'X'、'x' 为活细胞，其他字符为死细胞。
     * @param id 稳定的图案标识。
     * @param rows ASCII 行；长度不齐的行在右侧补空。
     * @return 构造出的图案。
     */
    static Pattern fromAscii(std::string id, const std::vector<std::string>& rows);

private:
    /// @brief 保存已校验的字段；调用方使用 fromCells()。
    /// @param id 稳定的图案标识。
    /// @param width 宽度（格）。
    /// @param height 高度（格）。
    /// @param cells 行主序细胞；大小必须等于 width * height。
    Pattern(std::string id, int width, int height, std::vector<Cell> cells);

    std::string m_id;          ///< 稳定的图案标识。
    int m_width = 0;           ///< 宽度（格）。
    int m_height = 0;          ///< 高度（格）。
    std::vector<Cell> m_cells; ///< 行主序细胞；size() == m_width * m_height。
};

} // namespace lifegame::domain
