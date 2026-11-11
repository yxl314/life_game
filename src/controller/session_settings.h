#pragma once

/**
 * @file session_settings.h
 * @brief Narrow port between session persistence and input/controller state.
 */

#include "shared/app_settings.h"

namespace lifegame::controller
{

/**
 * @brief Applies restored settings to controllers and captures current input state.
 *
 * SessionController owns persistence and the lifecycle; it must not know the
 * concrete BoardController/JumpController/SimulationController. The composition
 * root supplies an implementation of this port.
 */
class SessionSettingsPort
{
public:
    virtual ~SessionSettingsPort() = default;

    /// @param settings Settings to push into controllers and passive views.
    virtual void apply(const shared::AppSettings& settings) = 0;

    /// @return Current controller-owned input settings.
    virtual shared::AppSettings capture() const = 0;
};

} // namespace lifegame::controller
