#include "controller/controller_settings_bridge.h"

#include "controller/board_controller.h"
#include "controller/jump_controller.h"
#include "controller/simulation_controller.h"
#include "model/life_model.h"

namespace lifegame::controller
{

ControllerSettingsBridge::ControllerSettingsBridge(model::LifeModel* model,
                                                   SimulationController* simulation,
                                                   JumpController* jump,
                                                   BoardController* board)
    : m_model(model), m_simulation(simulation), m_jump(jump), m_board(board)
{
}

void ControllerSettingsBridge::apply(const shared::AppSettings& settings)
{
    if (m_simulation != nullptr)
    {
        m_simulation->setSpeedExternal(settings.speed);
    }
    if (m_jump != nullptr)
    {
        m_jump->setJumpStep(settings.jumpStep);
    }
    if (m_board != nullptr)
    {
        m_board->setSizeStep(settings.sizeStep);
        m_board->setRandomDensity(settings.density);
        m_board->setGridVisible(settings.gridVisible);
    }
}

shared::AppSettings ControllerSettingsBridge::capture() const
{
    shared::AppSettings settings;
    if (m_model != nullptr)
    {
        settings.snapshotInterval = m_model->snapshotInterval();
    }
    if (m_simulation != nullptr)
    {
        settings.speed = m_simulation->speed();
    }
    if (m_jump != nullptr)
    {
        settings.jumpStep = m_jump->jumpStep();
    }
    if (m_board != nullptr)
    {
        settings.sizeStep = m_board->sizeStep();
        settings.density = m_board->randomDensity();
        settings.gridVisible = m_board->gridVisible();
    }
    return settings;
}

} // namespace lifegame::controller
