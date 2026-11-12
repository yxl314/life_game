#pragma once

/**
 * @file board_limits.h
 * @brief 棋盘范围与默认尺寸等共享上限。
 */

namespace lifegame::domain
{

/**
 * @brief 由领域、模型、视图与持久化共享的棋盘上限。
 *
 * 把这些数值放在同一个头文件里，界面范围、RLE 上限与存储容量就不会各写
 * 一份而互相漂移。
 */
inline constexpr int kMinBoardExtent = 20; ///< 宽高的最小格数。
inline constexpr int kMaxBoardWidth = 1500; ///< 最大宽度（x 轴）格数。
inline constexpr int kMaxBoardHeight = 1000; ///< 最大高度（y 轴）格数。

inline constexpr int kDefaultBoardWidth = 250; ///< 默认棋盘宽度。
inline constexpr int kDefaultBoardHeight = 200; ///< 默认棋盘高度。

} // namespace lifegame::domain
