#pragma once

/**
 * @file command_result.h
 * @brief 模型命令的类型化结果。
 */

#include <QMetaType>

namespace lifegame::model
{

/**
 * @brief 模型命令的结果。
 *
 * 预期内的失败不会抛异常；控制器根据该结果码
 * 翻译成一条可操作的提示。
 */
enum class CommandResult
{
    Success = 0,       ///< 命令按请求完成。
    Busy,              ///< 模型正在异步计算或跳转，拒绝该命令。
    OutOfBounds,       ///< 坐标或下标超出棋盘。
    NoChange,          ///< 命令合法，但状态本来就是这样。
    InvalidRule,       ///< 规则记法或掩码非法。
    InvalidPattern,    ///< 图案为空或结构不一致。
    InvalidArgument,   ///< 空的或非法的 API 参数。
    InvalidTarget,     ///< 跳转目标为负数或等于当前代。
    NoHistory,         ///< 回退目标及其更早的代数没有可用快照。
    EmptyBoard,        ///< 空棋盘无法开始模拟。
    NotRunning,        ///< 暂停状态下请求单步。
    Clipped,           ///< 图案已放置，但被棋盘边缘裁剪。
    Adjusted,          ///< 请求的尺寸被夹取到支持范围。
    TooLarge,          ///< 文档或图案超过棋盘上限。
    OutOfMemory,       ///< 所需内存分配失败。
    ComputationFailed  ///< 后台单步或跳转计算失败。
};

/// @brief 判断命令是否真的生效。
/// @param result 命令结果。
/// @return 对 Success、Clipped 和 Adjusted 返回 true。
constexpr bool succeeded(CommandResult result) noexcept
{
    return result == CommandResult::Success || result == CommandResult::Clipped ||
           result == CommandResult::Adjusted;
}

/// @brief 判断命令是否未生效。
/// @param result 命令结果。
/// @return succeeded(result) 为 false 时返回 true。
constexpr bool failed(CommandResult result) noexcept
{
    return !succeeded(result);
}

/// @brief 返回用于日志和测试的稳定 ASCII 名称。
/// @param result 命令结果。
/// @return 名称字符串；不展示给用户。
constexpr const char* commandResultName(CommandResult result) noexcept
{
    switch (result)
    {
    case CommandResult::Success:
        return "Success";
    case CommandResult::Busy:
        return "Busy";
    case CommandResult::OutOfBounds:
        return "OutOfBounds";
    case CommandResult::NoChange:
        return "NoChange";
    case CommandResult::InvalidRule:
        return "InvalidRule";
    case CommandResult::InvalidPattern:
        return "InvalidPattern";
    case CommandResult::InvalidArgument:
        return "InvalidArgument";
    case CommandResult::InvalidTarget:
        return "InvalidTarget";
    case CommandResult::NoHistory:
        return "NoHistory";
    case CommandResult::EmptyBoard:
        return "EmptyBoard";
    case CommandResult::NotRunning:
        return "NotRunning";
    case CommandResult::Clipped:
        return "Clipped";
    case CommandResult::Adjusted:
        return "Adjusted";
    case CommandResult::TooLarge:
        return "TooLarge";
    case CommandResult::OutOfMemory:
        return "OutOfMemory";
    case CommandResult::ComputationFailed:
        return "ComputationFailed";
    }
    return "Unknown";
}

} // namespace lifegame::model

Q_DECLARE_METATYPE(lifegame::model::CommandResult)
