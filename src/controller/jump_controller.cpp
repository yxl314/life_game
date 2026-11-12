#include "controller/jump_controller.h"

#include <stacklog/stacklog.hpp>
#include "controller/error_text.h"

#include <algorithm>

#include "model/life_model.h"
#include "shared/app_settings.h"
#include "view/control_panel.h"
#include "view/interfaces.h"

namespace lifegame::controller
{

JumpController::JumpController(model::LifeModel* model,
                               view::ControlPanel* panel,
                               view::MessageSink* messages,
                               view::JumpProgressFactory factory,
                               QObject* parent)
    : QObject(parent),
      m_model(model),
      m_panel(panel),
      m_messages(messages),
      m_factory(std::move(factory))
{
    m_jumpStep = shared::AppSettings{}.jumpStep;
    m_panel->setJumpStep(m_jumpStep);
    connect(m_panel, &view::ControlPanel::jumpStepChanged, this, &JumpController::setJumpStep);
    connect(m_panel, &view::ControlPanel::rewindRequested, this, &JumpController::rewindBy);
    connect(m_panel, &view::ControlPanel::fastForwardRequested, this, &JumpController::jumpBy);
    connect(m_model, &model::LifeModel::jumpStarted, this, &JumpController::onJumpStarted);
    connect(m_model, &model::LifeModel::jumpProgress, this, &JumpController::onJumpProgress);
    connect(m_model, &model::LifeModel::jumpFinished, this, &JumpController::onJumpFinished);
    connect(m_model, &model::LifeModel::jumpCancelled, this, &JumpController::onJumpCancelled);
    connect(m_model, &model::LifeModel::jumpFailed, this, &JumpController::onJumpFailed);
}

void JumpController::rewind()
{
    jumpBy(-m_jumpStep);
}

void JumpController::rewindBy(int generations)
{
    jumpBy(-generations);
}

void JumpController::fastForward()
{
    jumpBy(m_jumpStep);
}

int JumpController::jumpStep() const
{
    return m_jumpStep;
}

void JumpController::setJumpStep(int step)
{
    m_jumpStep =
        std::clamp(step, shared::limits::kJumpStepMin, shared::limits::kJumpStepMax);
    m_panel->setJumpStep(m_jumpStep);
}

void JumpController::jumpBy(int delta)
{
    if (delta == 0 || m_model->isJumping())
    {
        return;
    }

    const int current = m_model->stats().generation;
    const int oldest = m_model->oldestGeneration();
    if (delta < 0 && current <= oldest)
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("已经是可回退的最早一代（第 %1 代）。").arg(oldest), 4000);
        }
        return;
    }

    int target = current + delta;
    if (target < oldest)
    {
        target = oldest;
    }
    if (target == current)
    {
        return;
    }

    const model::CommandResult result = m_model->startJump(target);
    STACKLOG_DEBUG() << "jump command delta=" << delta << " target=" << target
                     << " result=" << model::commandResultName(result);
    if (model::failed(result) && m_messages != nullptr)
    {
        m_messages->showMessage(commandResultText(result), 4000);
    }
}

void JumpController::onJumpStarted(const shared::JumpInfo& info)
{
    if (!m_factory)
    {
        STACKLOG_WARN() << "jump progress factory unavailable";
        return;
    }

    m_progress = m_factory();
    if (!m_progress)
    {
        STACKLOG_WARN() << "jump progress view unavailable; jump runs without cancel UI";
        return;
    }
    m_progress->setCancelHandler([this] { m_model->cancelJump(); });
    m_progress->show(info);
}

void JumpController::onJumpProgress(int done)
{
    if (m_progress)
    {
        m_progress->setProgress(done);
    }
}

void JumpController::onJumpFinished(int generation)
{
    closeProgress();
    if (m_messages != nullptr)
    {
        m_messages->showMessage(tr("跳转完成，当前第 %1 代。").arg(generation), 4000);
    }
}

void JumpController::onJumpCancelled()
{
    closeProgress();
    if (m_messages != nullptr)
    {
        m_messages->showMessage(tr("已取消跳转，仍停留在第 %1 代。").arg(m_model->stats().generation), 4000);
    }
}

void JumpController::onJumpFailed(model::CommandResult reason)
{
    closeProgress();
    if (m_messages != nullptr)
    {
        m_messages->showMessage(tr("跳转失败：%1").arg(commandResultText(reason)), 5000);
    }
}

void JumpController::closeProgress()
{
    if (!m_progress)
    {
        return;
    }
    m_progress->close();
    m_progress.reset();
}

} // namespace lifegame::controller
