#include "controller/simulation_controller.h"

#include "controller/error_text.h"

#include <QAction>
#include <QIcon>
#include <QTimer>

#include <algorithm>

#include "shared/app_settings.h"
#include "model/life_model.h"
#include "view/control_panel.h"
#include "view/interfaces.h"

#include <QSignalBlocker>

namespace lifegame::controller
{

namespace
{

int timerInterval(int generationsPerSecond)
{
    return std::max(1, 1000 / std::max(1, generationsPerSecond));
}

QString stallText(model::StallReason reason)
{
    switch (reason)
    {
    case model::StallReason::EmptyBoard:
        return SimulationController::tr("棋盘为空，请先绘制细胞或使用“随机填充”。");
    case model::StallReason::Extinct:
        return SimulationController::tr("所有细胞已死亡，模拟已自动停止。");
    case model::StallReason::StillLife:
        return SimulationController::tr("图案已稳定为静物，模拟已自动停止。");
    }
    return {};
}

} // namespace

SimulationController::SimulationController(model::LifeModel* model,
                                           view::ControlPanel* panel,
                                           view::MessageSink* messages,
                                           QObject* parent)
    : QObject(parent), m_model(model), m_panel(panel), m_messages(messages)
{
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_speed = shared::limits::kDefaultSpeed;
    m_panel->setSpeed(m_speed);

    connect(m_timer, &QTimer::timeout, this, &SimulationController::onTick);
    connect(m_panel, &view::ControlPanel::playPauseRequested, this, &SimulationController::toggleRunning);
    connect(m_panel, &view::ControlPanel::stepRequested, this, &SimulationController::stepOnce);
    connect(m_panel, &view::ControlPanel::speedChanged, this, &SimulationController::onSpeedChanged);
    connect(m_model, &model::LifeModel::runningChanged, this, &SimulationController::onRunningChanged);
    connect(m_model, &model::LifeModel::simulationStalled, this, &SimulationController::onStalled);
    connect(m_model, &model::LifeModel::stepFailed, this, [this](model::CommandResult reason) {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("模拟计算失败：%1").arg(commandResultText(reason)), 8000);
        }
    });
}

int SimulationController::speed() const
{
    return m_speed;
}

void SimulationController::setPlayPauseAction(QAction* action)
{
    m_playAction = action;
    updatePlayAction();
}

void SimulationController::setSpeedExternal(int speed)
{
    applySpeed(speed);
}

void SimulationController::applySpeed(int speed)
{
    m_speed = std::clamp(speed, shared::limits::kSpeedMin, shared::limits::kSpeedMax);
    m_panel->setSpeed(m_speed);
    emit speedChanged(m_speed);
    if (m_speed <= 0)
    {
        m_timer->stop();
        if (m_model->isRunning())
        {
            (void)m_model->setRunning(false);
        }
        return;
    }
    if (m_model->isRunning())
    {
        restartTimer();
    }
}

void SimulationController::toggleRunning()
{
    if (!m_model->isRunning() && m_speed <= 0)
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("速度为 0，请先调高速度再开始模拟。"), 4000);
        }
        return;
    }

    const bool before = m_model->isRunning();
    const model::CommandResult result = m_model->toggleRunning();
    if (result == model::CommandResult::EmptyBoard)
    {
        return; // simulationStalled already carries an explicit message
    }
    if (model::failed(result))
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(commandResultText(result), 4000);
        }
        return;
    }
    if (m_messages != nullptr && m_model->isRunning() != before)
    {
        m_messages->showMessage(m_model->isRunning() ? tr("模拟进行中…") : tr("模拟已暂停。"), 3000);
    }
}

void SimulationController::stepOnce()
{
    if (m_model->isJumping())
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("跳转进行中，暂不能单步。"), 3000);
        }
        return;
    }

    if (m_model->isRunning())
    {
        (void)m_model->setRunning(false);
    }
    const model::CommandResult result = m_model->step();
    if (m_messages != nullptr)
    {
        if (model::succeeded(result))
        {
            m_messages->showMessage(tr("单步演化到第 %1 代。").arg(m_model->stats().generation), 3000);
        }
        else
        {
            m_messages->showMessage(commandResultText(result), 4000);
        }
    }
}

void SimulationController::onTick()
{
    const model::CommandResult result = m_model->requestStep();
    // Busy/NotRunning are normal backpressure states; do not spam the status bar.
    if (result == model::CommandResult::OutOfMemory && m_messages != nullptr)
    {
        m_messages->showMessage(commandResultText(result), 4000);
    }
}

void SimulationController::onRunningChanged(bool running)
{
    if (running && m_speed > 0)
    {
        restartTimer();
    }
    else
    {
        m_timer->stop();
    }
    updatePlayAction();
}

void SimulationController::onSpeedChanged(int speed)
{
    applySpeed(speed);
    if (m_messages != nullptr)
    {
        if (m_speed == 0)
        {
            m_messages->showMessage(tr("速度为 0，模拟已暂停。"), 3000);
        }
        else
        {
            m_messages->showMessage(tr("模拟速度：%1 代/秒").arg(m_speed), 2500);
        }
    }
}

void SimulationController::onStalled(model::StallReason reason)
{
    if (m_messages != nullptr)
    {
        m_messages->showMessage(stallText(reason), 8000);
    }
}

void SimulationController::restartTimer()
{
    if (m_speed <= 0)
    {
        m_timer->stop();
        return;
    }
    if (m_model->isRunning())
    {
        m_timer->start(timerInterval(m_speed));
    }
}

void SimulationController::updatePlayAction()
{
    if (m_playAction == nullptr)
    {
        return;
    }

    const bool running = m_model->isRunning();
    const QSignalBlocker blocker(m_playAction);
    m_playAction->setText(running ? tr("暂停(&P)") : tr("开始(&P)"));
    m_playAction->setIconText(running ? tr("暂停") : tr("开始"));
    m_playAction->setToolTip(running ? tr("暂停模拟（空格）") : tr("开始模拟（空格）"));
    m_playAction->setIcon(QIcon(running ? QStringLiteral(":/icons/pause.svg")
                                        : QStringLiteral(":/icons/play.svg")));
}

} // namespace lifegame::controller
