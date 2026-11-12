#pragma once

/**
 * @file settings_store.h
 * @brief QSettings-backed persistence.
 */

#include <QString>

#include <memory>

#include "service/session_state.h"
#include "service/settings_error.h"

class QSettings;

namespace lifegame::service
{

/// @brief Result of loading persisted session data.
struct SettingsLoadResult
{
    SessionState state;                       ///< Loaded/normalized state.
    SettingsError error = SettingsError::None; ///< Load error, if any.
    QString detail;                           ///< Optional file/detail text.
};

/// @brief QSettings-backed persistence. No UI, no model state machine.
///
/// Key names are private to this class; callers only see SessionState. Every
/// scalar is range-checked while loading, so a hand-edited file degrades to
/// defaults plus SettingsError::CorruptData instead of failing the restore.
class SettingsStore
{
public:
    /// @brief Take ownership of the QSettings instance used for all I/O.
    ///
    /// @param settings Owning QSettings instance (may be null only in tests).
    explicit SettingsStore(std::unique_ptr<QSettings> settings);
    /// @brief Destroy the owned QSettings instance.
    ~SettingsStore();

    /// @brief The store owns its QSettings instance, so copying is disabled.
    SettingsStore(const SettingsStore&) = delete;
    /// @brief The store owns its QSettings instance, so assignment is disabled.
    SettingsStore& operator=(const SettingsStore&) = delete;

    /**
     * @brief Read and validate the stored session state.
     *
     * @return Loaded state, defaults, and a typed error if applicable.
     * @retval AccessError The store has no QSettings instance or the file is unreadable.
     * @retval FormatError QSettings rejected the file format.
     * @retval CorruptData A stored value was unusable and was replaced by a default.
     */
    SettingsLoadResult load() const;

    /**
     * @brief Persist session state.
     * @param state State to write.
     * @param detail Optional file/detail output.
     * @return SettingsError code.
     */
    SettingsError save(const SessionState& state, QString* detail = nullptr);

private:
    std::unique_ptr<QSettings> m_settings; ///< QSettings instance; null disables I/O.
};

} // namespace lifegame::service
