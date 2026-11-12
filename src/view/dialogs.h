#pragma once

/**
 * @file dialogs.h
 * @brief 本地化的消息框辅助函数。
 */

#include <QString>

class QWidget;

namespace lifegame::view
{

/// @brief 显示中文标签的警告框。
/// @param parent 对话框父窗口。
/// @param title 对话框标题。
/// @param text 警告正文。
void showWarning(QWidget* parent, const QString& title, const QString& text);

/// @brief 显示键鼠操作说明。
/// @param parent 对话框父窗口。
void showControlsHelp(QWidget* parent);

/// @brief 显示应用关于框。
/// @param parent 对话框父窗口。
/// @param version 应用版本。
void showAbout(QWidget* parent, const QString& version);

/// @brief 显示 Qt 关于框。
/// @param parent 对话框父窗口。
void showAboutQt(QWidget* parent);

} // namespace lifegame::view
