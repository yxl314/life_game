#pragma once

#include <algorithm>

/**
 * @file app_settings.h
 * @brief Cross-layer application settings and limits.
 */

namespace lifegame::shared
{

/// @brief Ranges shared by the settings dialog, session store and validation.
namespace limits
{
inline constexpr int kJumpStepMin = 100;
inline constexpr int kJumpStepMax = 1000;
inline constexpr int kSizeStepMin = 5;
inline constexpr int kSizeStepMax = 50;
inline constexpr int kSnapshotIntervalMin = 10;
inline constexpr int kSnapshotIntervalMax = 1000;
inline constexpr int kSpeedMin = 0;
inline constexpr int kSpeedMax = 120;
inline constexpr int kDefaultSpeed = 10;
inline constexpr int kDensityMin = 5;
inline constexpr int kDensityMax = 90;
inline constexpr int kDefaultDensity = 30;

/**
 * @brief Cell count above which the view uses image rendering and the
 * presenter may coalesce repaints when the screen cannot keep up.
 */
inline constexpr int kLargeBoardCellThreshold = 250000;
} // namespace limits

/// @brief User-adjustable application settings persisted with the session.
struct AppSettings
{
    int jumpStep = 100;                    ///< Jump amount in generations.
    int sizeStep = 5;                      ///< Board-size spinner step.
    int snapshotInterval = 100;            ///< Snapshot interval in generations.
    int density = limits::kDefaultDensity; ///< Random-fill percentage.
    int speed = limits::kDefaultSpeed;     ///< Generations per second (0 = pause).
    bool gridVisible = true;               ///< Whether grid lines are shown.
};

/**
 * @brief Clamp every field to its supported range.
 * @param settings Input settings.
 * @return Normalized settings.
 */
constexpr AppSettings normalized(const AppSettings& settings) noexcept
{
    AppSettings result = settings;
    result.jumpStep = std::clamp(result.jumpStep, limits::kJumpStepMin, limits::kJumpStepMax);
    result.sizeStep = std::clamp(result.sizeStep, limits::kSizeStepMin, limits::kSizeStepMax);
    result.snapshotInterval =
        std::clamp(result.snapshotInterval, limits::kSnapshotIntervalMin, limits::kSnapshotIntervalMax);
    result.density = std::clamp(result.density, limits::kDensityMin, limits::kDensityMax);
    result.speed = std::clamp(result.speed, limits::kSpeedMin, limits::kSpeedMax);
    return result;
}

} // namespace lifegame::shared
