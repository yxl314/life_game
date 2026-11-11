#pragma once

/**
 * @file controller_settings_bridge.h
 * @brief Composition helper that adapts concrete controllers to SessionSettingsPort.
 */

#include "controller/session_settings.h"

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::controller
{

class BoardController;
class JumpController;
class SimulationController;

/**
 * @brief Applies settings to the input controllers and captures their state.
 *
 * This is deliberately a controller-layer adapter, not a second application
 * root: it only forwards values between the narrow SessionSettingsPort and the
 * concrete controllers.
 */
class ControllerSettingsBridge : public SessionSettingsPort
{
public:
    /// @param model Model whose snapshot interval is captured.
    /// @param simulation Simulation speed owner.
    /// @param jump Jump-step owner.
    /// @param board Density/size/grid ownership.
    ControllerSettingsBridge(model::LifeModel* model,
                             SimulationController* simulation,
                             JumpController* jump,
                             BoardController* board);

    /// @param settings Settings to apply to each controller.
    void apply(const shared::AppSettings& settings) override;

    /// @return Complete snapshot of controller-owned settings.
    shared::AppSettings capture() const override;

private:
    model::LifeModel* m_model = nullptr;
    SimulationController* m_simulation = nullptr;
    JumpController* m_jump = nullptr;
    BoardController* m_board = nullptr;
};

} // namespace lifegame::controller
