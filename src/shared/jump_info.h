#pragma once

/**
 * @file jump_info.h
 * @brief 回退/快进进度的值对象。
 */

namespace lifegame::shared
{

/// @brief 一次回退/快进操作的参数。
struct JumpInfo
{
    int fromGeneration = 0;    ///< 重放起点所在代数。
    int toGeneration = 0;      ///< 请求的目标代数。
    int replayGenerations = 0; ///< 需要重放的代数。
};

} // namespace lifegame::shared
