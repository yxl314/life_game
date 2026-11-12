#pragma once

#include <algorithm>

/**
 * @file app_settings.h
 * @brief 跨层共享的应用设置与范围上限。
 */

namespace lifegame::shared
{

/// @brief 设置对话框、会话存储与校验共用的取值范围。
namespace limits
{
inline constexpr int kJumpStepMin = 100;            ///< 跳转粒度的下限。
inline constexpr int kJumpStepMax = 1000;           ///< 跳转粒度的上限。
inline constexpr int kSizeStepMin = 5;              ///< 尺寸粒度的下限。
inline constexpr int kSizeStepMax = 50;             ///< 尺寸粒度的上限。
inline constexpr int kSnapshotIntervalMin = 10;     ///< 快照间隔的下限。
inline constexpr int kSnapshotIntervalMax = 1000;   ///< 快照间隔的上限。
inline constexpr int kSpeedMin = 0;                 ///< 速度下限（0 表示暂停）。
inline constexpr int kSpeedMax = 120;               ///< 速度上限。
inline constexpr int kDefaultSpeed = 10;            ///< 默认速度。
inline constexpr int kDensityMin = 5;               ///< 随机填充密度的下限。
inline constexpr int kDensityMax = 90;              ///< 随机填充密度的上限。
inline constexpr int kDefaultDensity = 30;          ///< 默认随机填充密度。

/**
 * @brief 超过该格数后，视图改用图像渲染，呈现器可以在屏幕跟不上时合并重绘。
 */
inline constexpr int kLargeBoardCellThreshold = 250000;
} // namespace limits

/// @brief 随会话持久化的用户可调设置。
struct AppSettings
{
    int jumpStep = 100;                    ///< 跳转代数。
    int sizeStep = 5;                      ///< 棋盘尺寸步进粒度。
    int snapshotInterval = 100;            ///< 快照间隔（代）。
    int density = limits::kDefaultDensity; ///< 随机填充百分比。
    int speed = limits::kDefaultSpeed;     ///< 每秒代数（0 表示暂停）。
    bool gridVisible = true;               ///< 是否显示网格线。
};

/// @brief 把每个字段钳制到各自的支持范围内。
/// @param settings 输入设置。
/// @return 归一化后的设置。
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
