#pragma once

/**
 * @file stall_reason.h
 * @brief 模拟自行停止的原因。
 */

#include <QMetaType>

namespace lifegame::model
{

/// @brief 模拟自行停止的原因。
enum class StallReason
{
    EmptyBoard = 0, ///< 没有存活细胞。
    Extinct,        ///< 演化过程中所有细胞死亡。
    StillLife       ///< 演化后棋盘没有变化。
};

/// @brief 返回用于日志和测试的稳定 ASCII 名称。
/// @param reason 停滞原因。
/// @return 名称字符串。
constexpr const char* stallReasonName(StallReason reason) noexcept
{
    switch (reason)
    {
    case StallReason::EmptyBoard:
        return "empty_board";
    case StallReason::Extinct:
        return "extinct";
    case StallReason::StillLife:
        return "still_life";
    }
    return "unknown";
}

} // namespace lifegame::model

Q_DECLARE_METATYPE(lifegame::model::StallReason)
