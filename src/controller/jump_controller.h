#pragma once

/**
 * @file jump_controller.h
 * @brief Rewind/fast-forward controller and progress UI.
 */

#include <QObject>

#include <memory>

#include "model/command_result.h"
#include "view/interfaces.h"

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

/// @brief Rewind/fast-forward input and progress presentation.
class JumpController : public QObject
{
    Q_OBJECT

public:
    /// @param model Model to command.
    /// @param panel Jump controls.
    /// @param messages Status output port.
    /// @param factory Progress-view factory (may be empty in tests).
    /// @param parent QObject parent.
    JumpController(model::LifeModel* model,
                   view::ControlPanel* panel,
                   view::MessageSink* messages,
                   view::JumpProgressFactory factory,
                   QObject* parent = nullptr);

public slots:
    /// @brief Rewind by the panel's jump amount.
    void rewind();
    /// @brief Fast-forward by the panel's jump amount.
    void fastForward();
    /// @param delta Positive forward, negative backward.
    void jumpBy(int delta);

    /// @return Current jump amount in generations.
    int jumpStep() const;

    /// @param step Jump amount in generations; clamped to the settings range.
    void setJumpStep(int step);

private slots:
    void rewindBy(int generations);
    void onJumpStarted(const lifegame::shared::JumpInfo& info);
    void onJumpProgress(int done);
    void onJumpFinished(int generation);
    void onJumpCancelled();
    void onJumpFailed(lifegame::model::CommandResult reason);

private:
    void closeProgress();

    model::LifeModel* m_model = nullptr;
    view::ControlPanel* m_panel = nullptr;
    view::MessageSink* m_messages = nullptr;
    view::JumpProgressFactory m_factory;
    std::unique_ptr<view::JumpProgressView> m_progress;
    int m_jumpStep = 100;
};

} // namespace lifegame::controller
