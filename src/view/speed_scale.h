#pragma once

/**
 * @file speed_scale.h
 * @brief 滑块位置与每秒代数之间的映射。
 */

namespace lifegame::view
{

/// @brief 速度滑块的最大位置。
inline constexpr int kSpeedSliderSteps = 100;

/// @brief 把滑块位置换算成每秒演化代数。
/// @param position 滑块位置，范围 [0, 100]。
/// @return 每秒演化代数，范围 [0, 120]；0 表示暂停。
int speedForSliderPosition(int position);

/// @brief 把每秒演化代数换算成滑块位置。
/// @param speed 每秒演化代数。
/// @return 滑块位置，范围 [0, 100]。
int sliderPositionForSpeed(int speed);

} // namespace lifegame::view
