#include "controller/board_presenter.h"

#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QWindow>

#include <algorithm>
#include <cmath>

#include "model/life_model.h"
#include "shared/app_settings.h"
#include "view/board_view.h"
#include "view/control_panel.h"
#include "view/view_state.h"

namespace lifegame::controller
{

BoardPresenter::BoardPresenter(model::LifeModel* model,
                               view::BoardView* view,
                               view::ControlPanel* panel,
                               view::FrameTimingSource* timing,
                               QObject* parent)
    : QObject(parent), m_model(model), m_view(view), m_panel(panel), m_timing(timing)
{
    refreshScreenRate();

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(std::max(1, 1000 / m_screenRefresh));
    connect(m_timer, &QTimer::timeout, this, &BoardPresenter::updateNow);

    connect(m_model, &model::LifeModel::boardChanged, this, &BoardPresenter::requestUpdate);
    connect(m_model, &model::LifeModel::statusChanged, this, &BoardPresenter::requestUpdate);
    connect(m_model, &model::LifeModel::configurationChanged, this, &BoardPresenter::requestUpdate);
    connect(m_model, &model::LifeModel::boardResized, this, &BoardPresenter::updateNow);
    connect(m_model, &model::LifeModel::runningChanged, this, [this](bool running) {
        if (!running && m_timer->isActive())
        {
            m_timer->stop();
        }
        requestUpdate();
    });

    if (QGuiApplication* app = qobject_cast<QGuiApplication*>(QCoreApplication::instance()))
    {
        connect(app, &QGuiApplication::primaryScreenChanged, this, [this] {
            refreshScreenRate();
            if (m_model->isRunning())
            {
                updateNow();
            }
        });
    }

    QTimer::singleShot(0, this, &BoardPresenter::watchWindowScreen);
    sync();
}

void BoardPresenter::watchWindowScreen()
{
    QWindow* window = m_view->windowHandle();
    if (window == nullptr)
    {
        return;
    }
    if (m_windowScreenConnection)
    {
        disconnect(m_windowScreenConnection);
    }
    m_windowScreenConnection = connect(window, &QWindow::screenChanged, this, [this](QScreen*) {
        refreshScreenRate();
        updateNow();
    });
    refreshScreenRate();
}

void BoardPresenter::refreshScreenRate()
{
    QScreen* screen = nullptr;
    if (QWindow* window = m_view->windowHandle())
    {
        screen = window->screen();
    }
    if (screen == nullptr)
    {
        screen = QGuiApplication::primaryScreen();
    }

    m_screenRefresh = 60;
    if (screen != nullptr)
    {
        const qreal refresh = screen->refreshRate();
        if (refresh > 0.0)
        {
            m_screenRefresh = std::max(1, static_cast<int>(std::lround(refresh)));
        }
    }
    if (m_timer != nullptr)
    {
        m_timer->setInterval(std::max(1, 1000 / m_screenRefresh));
    }

    if (m_screenConnection)
    {
        disconnect(m_screenConnection);
    }
    if (screen != nullptr)
    {
        m_screenConnection =
            connect(screen, &QScreen::refreshRateChanged, this, [this](qreal) { refreshScreenRate(); });
    }
}

void BoardPresenter::setGenerationRate(int generationsPerSecond)
{
    m_generationRate = std::max(0, generationsPerSecond);
    if (m_timer->isActive() && !needsCoalescing())
    {
        m_timer->stop();
        updateNow();
    }
}

bool BoardPresenter::needsCoalescing() const
{
    const int cells = m_model->width() * m_model->height();
    if (!m_model->isRunning() || cells <= shared::limits::kLargeBoardCellThreshold)
    {
        return false;
    }

    const double frameBudgetMs = 1000.0 / static_cast<double>(std::max(1, m_screenRefresh));
    const double paintMs = m_timing != nullptr ? m_timing->lastPaintDurationMs() : 0.0;
    return m_generationRate > m_screenRefresh || (paintMs > 0.0 && paintMs > frameBudgetMs);
}

void BoardPresenter::requestUpdate()
{
    if (!needsCoalescing())
    {
        updateNow();
        return;
    }

    if (!m_timer->isActive())
    {
        m_timer->start();
    }
}

void BoardPresenter::updateNow()
{
    if (m_timer != nullptr)
    {
        m_timer->stop();
    }

    const shared::LifeStats stats = m_model->stats();
    view::BoardFrame frame;
    frame.board = &m_model->board();
    frame.population = stats.population;
    frame.running = m_model->isRunning();
    m_view->applyFrame(frame);

    view::PanelState panel;
    panel.stats = stats;
    panel.width = m_model->width();
    panel.height = m_model->height();
    panel.ruleNotation = QString::fromStdString(m_model->rule().notation());
    panel.boundary = m_model->boundary();
    panel.oldestGeneration = m_model->oldestGeneration();
    panel.running = m_model->isRunning();
    m_panel->applyState(panel);
}

void BoardPresenter::sync()
{
    updateNow();
}

} // namespace lifegame::controller
