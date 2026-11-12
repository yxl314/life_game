#pragma once

/**
 * @file controller_settings_bridge.h
 * @brief 把具体控制器适配到 SessionSettingsPort 的组装辅助类。
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
 * @brief 把设置下发给输入控制器，并把它们的当前值采回来。
 *
 * 这是控制器层的适配器，不是第二个应用根：它只在窄接口
 * SessionSettingsPort 与具体控制器之间转发数值。
 */
class ControllerSettingsBridge : public SessionSettingsPort
{
public:
    /// @brief 绑定持有被持久化输入设置的控制器。
    /// @param model 提供快照间隔的模型。
    /// @param simulation 速度持有者。
    /// @param jump 跳转粒度持有者。
    /// @param board 密度/尺寸/网格持有者。
    ControllerSettingsBridge(model::LifeModel* model,
                             SimulationController* simulation,
                             JumpController* jump,
                             BoardController* board);

    /// @brief 把设置下发给各控制器；空成员跳过。
    /// @param settings 要下发给各控制器的设置。
    void apply(const shared::AppSettings& settings) override;

    /// @brief 采集各控制器持有的设置。
    ///
    /// 持有者为空的字段保留 AppSettings 的默认值。
    /// @return 控制器侧设置的完整快照。
    shared::AppSettings capture() const override;

private:
    model::LifeModel* m_model = nullptr;          ///< 快照间隔的持有者。
    SimulationController* m_simulation = nullptr; ///< 速度的持有者。
    JumpController* m_jump = nullptr;             ///< 跳转粒度的持有者。
    BoardController* m_board = nullptr;           ///< 密度/尺寸/网格的持有者。
};

} // namespace lifegame::controller
