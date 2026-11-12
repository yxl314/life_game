#pragma once

/**
 * @file jump_runner.h
 * @brief 后台回退/快进重放。
 */

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "domain/grid.h"
#include "domain/packed_grid.h"
#include "domain/rule.h"
#include "model/command_result.h"

namespace lifegame::model
{

/// @brief 重放过程中产生的一张打包快照。
struct JumpSnapshot
{
    int generation = 0; ///< 快照对应的代数。
    std::shared_ptr<const domain::PackedGrid> packedBoard; ///< 打包后的棋盘。
};

/// @brief 后台回退/快进重放的结果。
struct JumpOutcome
{
    std::shared_ptr<const domain::Grid> board; ///< 最终棋盘的副本。
    std::vector<JumpSnapshot> snapshots;        ///< 重放期间收集的新快照。
    int generation = 0;                         ///< 目标代数。
    bool cancelled = false;                     ///< 观察到取消标志时为 true。
    CommandResult error = CommandResult::Success; ///< 分配或计算失败时设置。
};

/// @brief 重放工作线程的不可变请求。
struct JumpRequest
{
    std::shared_ptr<const domain::Grid> startBoard; ///< startGeneration 对应的棋盘。
    int startGeneration = 0;  ///< 起点已经代表的代数。
    int targetGeneration = 0; ///< 请求的最终代数。
    int totalGenerations = 0; ///< target - start。
    bool forward = false;     ///< 为 true 时收集新快照。
    domain::Rule rule;        ///< 重放使用的规则。
    domain::Boundary boundary = domain::Boundary::Dead; ///< 边界策略。
    int epoch = 0;            ///< 快照纪元的起点代数。
    int interval = 100;       ///< 快照间隔。
    std::shared_ptr<std::atomic_bool> cancel; ///< 协作取消标志。
    std::function<void(int)> reportProgress; ///< 可选的进度回调（工作线程）。
};

/**
 * @brief 在当前工作线程上执行一次回退/快进重放。
 * @param request 不可变的重放请求。
 * @return 最终棋盘、新快照或类型化错误。
 * @note 调用方必须在工作线程运行；进度回调也在该线程触发。
 */
JumpOutcome runJump(const JumpRequest& request) noexcept;

} // namespace lifegame::model
