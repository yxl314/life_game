#pragma once

/**
 * @file session_settings.h
 * @brief 会话持久化与输入/控制器状态之间的窄端口。
 */

#include "shared/app_settings.h"

namespace lifegame::controller
{

/**
 * @brief 把恢复的设置下发给控制器，并采集当前的输入状态。
 *
 * SessionController 负责持久化与生命周期，不应认识具体的
 * BoardController/JumpController/SimulationController；由组装根提供本端口的
 * 实现。
 */
class SessionSettingsPort
{
public:
    /// @brief 允许通过端口接口删除实现。
    virtual ~SessionSettingsPort() = default;

    /// @brief 把设置下发给控制器与被动视图。
    /// @param settings 要下发的设置。
    virtual void apply(const shared::AppSettings& settings) = 0;

    /// @brief 采集控制器侧当前的输入设置。
    /// @return 当前输入设置。
    virtual shared::AppSettings capture() const = 0;
};

} // namespace lifegame::controller
