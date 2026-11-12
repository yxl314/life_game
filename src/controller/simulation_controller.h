#pragma once

/**
 * @file simulation_controller.h
 * @brief 模拟时钟与开始/暂停、单步、速度的控制。
 */

#include <QObject>

#include "model/stall_reason.h"
#include "shared/app_settings.h"

class QTimer;
class QAction;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class ControlPanel;
class MessageSink;
}

namespace lifegame::controller
{

/**
 * @brief 模拟时钟与开始/暂停、单步、速度意图。
 *
 * 本类持有驱动步进的定时器，但“正在运行”只由模型持有。速度为 0 视为
 * 不可运行：停表、暂停模型，并拒绝启动模拟（给出提示而不是挂一只极慢
 * 的定时器）。空闲与未运行等背压状态不刷状态栏。
 */
class SimulationController : public QObject
{
    Q_OBJECT

public:
    /// @brief 创建步进定时器，并连接面板意图与模型信号。
    /// @param model 被驱动的模型。
    /// @param panel 速度与播放控件。
    /// @param messages 状态栏输出端口。
    /// @param parent QObject 父对象。
    SimulationController(model::LifeModel* model,
                         view::ControlPanel* panel,
                         view::MessageSink* messages,
                         QObject* parent = nullptr);

    /// @brief 查询当前速率。
    /// @return 每秒代数；0 表示暂停。
    int speed() const;

    /// @brief 让菜单/工具栏动作跟随运行状态。
    /// @param action 需要同步的开始/暂停动作。
    void setPlayPauseAction(QAction* action);

    /// @brief 恢复已保存的速率，不产生用户提示。
    /// @param speed 恢复的速率；为 0 时暂停。
    void setSpeedExternal(int speed);

signals:
    /// @brief 速率被钳制并生效后发出。
    /// @param speed 新的每秒代数。
    void speedChanged(int speed);

public slots:
    /// @brief 切换运行状态，并遵守零速率不启动的规则。
    void toggleRunning();
    /// @brief 暂停后同步演化一代。
    void stepOnce();

private slots:
    /// @brief 向模型请求一步；只把内存不足报告给用户。
    void onTick();
    /// @brief 按模型的运行标志启动或停止定时器。
    /// @param running 新的运行状态。
    void onRunningChanged(bool running);
    /// @brief 应用面板选择的速度并给出提示。
    /// @param speed 新的每秒代数。
    void onSpeedChanged(int speed);
    /// @brief 报告模型停止模拟的原因。
    /// @param reason 模型给出的停滞原因。
    void onStalled(lifegame::model::StallReason reason);

private:
    /// @brief 钳制并应用速率；为 0 时暂停模型。
    /// @param speed 请求的每秒代数。
    void applySpeed(int speed);
    /// @brief 按当前速率重启定时器；暂停或速率为 0 时停表。
    void restartTimer();
    /// @brief 刷新开始/暂停动作的文字、提示与图标。
    void updatePlayAction();

    model::LifeModel* m_model = nullptr;     ///< 被驱动的模型。
    view::ControlPanel* m_panel = nullptr;   ///< 速度与播放控件。
    view::MessageSink* m_messages = nullptr; ///< 状态栏输出端口。
    QTimer* m_timer = nullptr;               ///< 步进定时器，由 QObject 父子关系持有。
    int m_speed = shared::limits::kDefaultSpeed; ///< 每秒代数。
    QAction* m_playAction = nullptr;         ///< 需同步的开始/暂停动作，可为空。
};

} // namespace lifegame::controller
