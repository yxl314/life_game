#pragma once

/**
 * @file theme.h
 * @brief 深色 Fusion 主题与应用图标。
 */

#include <QIcon>

class QApplication;

namespace lifegame::view::theme
{

/// @brief 应用深色 Fusion 外观；必须在创建窗口前调用。
/// @param app 应用实例。
void apply(QApplication& app);

/// @brief 生成程序化绘制的应用图标。
/// @return 应用图标。
QIcon appIcon();

} // namespace lifegame::view::theme
