#pragma once

/**
 * @file settings_error.h
 * @brief Typed settings persistence errors.
 */

namespace lifegame::service
{

/// @brief Recoverable failures of loading or saving the session file.
enum class SettingsError
{
    None = 0,      ///< Success.
    AccessError,   ///< File cannot be read/written.
    FormatError,   ///< QSettings format is invalid.
    CorruptData    ///< One or more values are malformed/clamped.
};

/// @brief Stable spelling of an error code for logs.
///
/// @param error Error code.
/// @return Static name; "Unknown" for a value outside the enumeration.
constexpr const char* settingsErrorName(SettingsError error) noexcept
{
    switch (error)
    {
    case SettingsError::None:
        return "None";
    case SettingsError::AccessError:
        return "AccessError";
    case SettingsError::FormatError:
        return "FormatError";
    case SettingsError::CorruptData:
        return "CorruptData";
    }
    return "Unknown";
}

} // namespace lifegame::service
