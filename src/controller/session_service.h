#pragma once

/**
 * @file session_service.h
 * @brief UI-free session persistence service.
 */

#include <QByteArray>
#include <QString>

#include "model/command_result.h"
#include "service/settings_error.h"
#include "shared/app_settings.h"

namespace lifegame::domain
{
struct RleErrorInfo;
}

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class SettingsStore;
}

namespace lifegame::controller
{

/// @brief Outcome of restoring persisted session data.
enum class SessionRestoreStatus
{
    NoStoredBoard, ///< No board was persisted; settings may still be loaded.
    Restored,      ///< Board and generation were restored.
    PartialRestore,///< Board was restored, but the generation could not be applied.
    DecodeFailed,  ///< Stored RLE was malformed.
    LoadFailed     ///< Model rejected the decoded document or fallback allocation failed.
};

/// @brief Result of SessionService::restore().
struct SessionRestoreResult
{
    shared::AppSettings settings;              ///< Normalized settings.
    service::SettingsError error = service::SettingsError::None; ///< Settings error.
    QString detail;                            ///< Optional error detail.
    SessionRestoreStatus status = SessionRestoreStatus::NoStoredBoard; ///< Restore outcome.
    model::CommandResult modelResult = model::CommandResult::Success;  ///< Model failure detail.
    QByteArray windowGeometry;                 ///< Saved window geometry.

    /// @return True when the persisted state contained a board payload.
    bool hadStoredBoard() const noexcept
    {
        return status != SessionRestoreStatus::NoStoredBoard;
    }

    /// @return True when at least the board was restored.
    bool restored() const noexcept
    {
        return status == SessionRestoreStatus::Restored ||
               status == SessionRestoreStatus::PartialRestore;
    }
};

/// @brief UI-free session persistence.
class SessionService
{
public:
    /// @param model Model to restore/save.
    /// @param store Persistence store.
    SessionService(model::LifeModel* model, service::SettingsStore* store);

    /// @return Restore result and fallback board state.
    SessionRestoreResult restore();

    /**
     * @brief Persist current model state and settings.
     * @param settings Settings to store.
     * @param windowGeometry Window geometry to store.
     * @param detail Optional error detail.
     * @return Settings error code.
     */
    service::SettingsError save(const shared::AppSettings& settings,
                                const QByteArray& windowGeometry,
                                QString* detail = nullptr);

private:
    model::LifeModel* m_model = nullptr;
    service::SettingsStore* m_store = nullptr;
};

} // namespace lifegame::controller
