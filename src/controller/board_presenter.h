#pragma once

/**
 * @file board_presenter.h
 * @brief Model-to-view presenter and frame scheduler.
 */

#include <QMetaObject>
#include <QObject>

#include "shared/app_settings.h"

class QTimer;
class QScreen;
class QWindow;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class BoardView;
class ControlPanel;
class FrameTimingSource;
}

namespace lifegame::controller
{

/**
 * @brief Pushes Model state into passive views and owns the frame scheduler.
 *
 * BoardController handles input/commands only; this presenter owns the
 * Model->View direction and the decision to coalesce repaints.
 */
class BoardPresenter : public QObject
{
    Q_OBJECT

public:
    /// @param model Model to observe.
    /// @param view Passive board view.
    /// @param panel Passive side panel.
    /// @param timing Narrow paint-duration metric source.
    /// @param parent QObject parent.
    BoardPresenter(model::LifeModel* model,
                   view::BoardView* view,
                   view::ControlPanel* panel,
                   view::FrameTimingSource* timing,
                   QObject* parent = nullptr);

    /// @brief Sync with current model state, bypassing the frame timer.
    void sync();

    /// @param generationsPerSecond Current simulation rate.
    void setGenerationRate(int generationsPerSecond);

private slots:
    void requestUpdate();
    void updateNow();

private:
    void watchWindowScreen();
    void refreshScreenRate();
    bool needsCoalescing() const;

    model::LifeModel* m_model = nullptr;
    view::BoardView* m_view = nullptr;
    view::ControlPanel* m_panel = nullptr;
    view::FrameTimingSource* m_timing = nullptr;
    QTimer* m_timer = nullptr;
    int m_screenRefresh = 60;
    int m_generationRate = shared::limits::kDefaultSpeed;
    QMetaObject::Connection m_screenConnection;
    QMetaObject::Connection m_windowScreenConnection;
};

} // namespace lifegame::controller
