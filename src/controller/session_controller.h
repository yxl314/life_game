#pragma once

/**
 * @file session_controller.h
 * @brief Session restore/save and settings lifecycle coordinator.
 */

#include <QObject>

#include "shared/app_settings.h"
#include "service/settings_error.h"

#include <memory>

class QMainWindow;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class SettingsStore;
}

namespace lifegame::view
{
class MessageSink;
}

namespace lifegame::controller
{

class SessionService;
class SessionSettingsPort;

/// @brief Application/session coordinator: restore on start, persist on exit.
class SessionController : public QObject
{
    Q_OBJECT

public:
    /// @param model Model to restore/save.
    /// @param settingsPort Narrow adapter to controller-owned input settings.
    /// @param store Persistence store.
    /// @param window Window geometry source.
    /// @param messages Status output port.
    /// @param parent QObject parent.
    SessionController(model::LifeModel* model,
                      SessionSettingsPort* settingsPort,
                      service::SettingsStore* store,
                      QMainWindow* window,
                      view::MessageSink* messages,
                      QObject* parent = nullptr);
    ~SessionController() override;

    /// @return True when a persisted board was restored; falls back to default board.
    bool restore();

    /// @return Save result code.
    service::SettingsError save();

    /// @return Last applied/normalized settings.
    const shared::AppSettings& settings() const;

    /// @brief Capture current controller-owned input state into settings().
    void refreshSettings();

    /// @param values Settings to normalize and apply.
    void applySettings(const shared::AppSettings& values);

private:
    model::LifeModel* m_model = nullptr;
    SessionSettingsPort* m_settingsPort = nullptr;
    std::unique_ptr<SessionService> m_service;
    QMainWindow* m_window = nullptr;
    view::MessageSink* m_messages = nullptr;
    shared::AppSettings m_settings;
};

} // namespace lifegame::controller
