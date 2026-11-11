#include "controller/session_controller.h"

#include <stacklog/stacklog.h>
#include "controller/error_text.h"
#include "controller/session_service.h"
#include "controller/session_settings.h"

#include <QMainWindow>

#include "model/life_model.h"
#include "view/interfaces.h"

namespace lifegame::controller
{

SessionController::SessionController(model::LifeModel* model,
                                     SessionSettingsPort* settingsPort,
                                     service::SettingsStore* store,
                                     QMainWindow* window,
                                     view::MessageSink* messages,
                                     QObject* parent)
    : QObject(parent),
      m_model(model),
      m_settingsPort(settingsPort),
      m_service(std::make_unique<SessionService>(model, store)),
      m_window(window),
      m_messages(messages)
{
}

SessionController::~SessionController() = default;

const shared::AppSettings& SessionController::settings() const
{
    return m_settings;
}

void SessionController::refreshSettings()
{
    if (m_settingsPort != nullptr)
    {
        m_settings = shared::normalized(m_settingsPort->capture());
    }
}

bool SessionController::restore()
{
    const SessionRestoreResult result = m_service->restore();
    m_settings = result.settings;

    STACKLOG_INFO() << "session restore start has_stored_board=" << result.hadStoredBoard()
                    << " restored=" << result.restored();
    if (result.error != service::SettingsError::None)
    {
        STACKLOG_WARN() << "settings restore failed error="
                        << service::settingsErrorName(result.error);
        if (m_messages != nullptr)
        {
            m_messages->showMessage(settingsErrorText(result.error, result.detail), 8000);
        }
    }

    switch (result.status)
    {
    case SessionRestoreStatus::NoStoredBoard:
    case SessionRestoreStatus::Restored:
        break;
    case SessionRestoreStatus::PartialRestore:
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("上次会话的棋盘已恢复，但代数未能恢复。"), 8000);
        }
        break;
    case SessionRestoreStatus::DecodeFailed:
        STACKLOG_WARN() << "stored RLE decode failed";
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("上次会话的棋盘无法解析，已改为示例棋盘。"), 8000);
        }
        break;
    case SessionRestoreStatus::LoadFailed:
        if (m_messages != nullptr)
        {
            m_messages->showMessage(
                tr("上次会话的棋盘无法恢复：%1 已改为示例棋盘。")
                    .arg(commandResultText(result.modelResult)),
                8000);
        }
        break;
    }

    if (m_settingsPort != nullptr)
    {
        m_settingsPort->apply(m_settings);
    }

    if (m_window != nullptr && !result.windowGeometry.isEmpty() &&
        !m_window->restoreGeometry(result.windowGeometry))
    {
        STACKLOG_DEBUG() << "window geometry restore failed";
    }

    STACKLOG_INFO() << "session restore done restored=" << result.restored() << " board="
                    << m_model->width() << 'x' << m_model->height() << " generation "
                    << m_model->stats().generation;
    return result.restored();
}

service::SettingsError SessionController::save()
{
    refreshSettings();

    const QByteArray geometry = m_window != nullptr ? m_window->saveGeometry() : QByteArray();
    QString detail;
    const service::SettingsError error = m_service->save(m_settings, geometry, &detail);
    if (error != service::SettingsError::None)
    {
        STACKLOG_WARN() << "session save failed error=" << service::settingsErrorName(error);
        if (m_messages != nullptr)
        {
            m_messages->showMessage(settingsErrorText(error, detail), 8000);
        }
    }
    else
    {
        STACKLOG_INFO() << "session saved board=" << m_model->width() << 'x' << m_model->height()
                        << " generation " << m_model->stats().generation;
    }
    return error;
}

void SessionController::applySettings(const shared::AppSettings& values)
{
    m_settings = shared::normalized(values);
    const model::CommandResult result = m_model->setSnapshotInterval(m_settings.snapshotInterval);
    if (model::failed(result) && result != model::CommandResult::NoChange && m_messages != nullptr)
    {
        m_messages->showMessage(tr("设置应用失败：%1").arg(commandResultText(result)), 4000);
    }
    if (m_settingsPort != nullptr)
    {
        m_settingsPort->apply(m_settings);
    }
    STACKLOG_INFO() << "settings applied jump_step=" << m_settings.jumpStep << " size_step="
                    << m_settings.sizeStep << " snapshot_interval=" << m_settings.snapshotInterval;
}

} // namespace lifegame::controller
