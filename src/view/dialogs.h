#pragma once

/**
 * @file dialogs.h
 * @brief Localized message-box helpers.
 */

#include <QString>

class QWidget;

namespace lifegame::view
{

/// @brief Show a Chinese-labeled warning box.
/// @param parent Dialog parent.
/// @param title Dialog title.
/// @param text Warning text.
void showWarning(QWidget* parent, const QString& title, const QString& text);

/// @brief Show the keyboard/mouse controls help.
/// @param parent Dialog parent.
void showControlsHelp(QWidget* parent);

/// @brief Show the application about box.
/// @param parent Dialog parent.
/// @param version Application version.
void showAbout(QWidget* parent, const QString& version);

/// @brief Show the Qt about box.
/// @param parent Dialog parent.
void showAboutQt(QWidget* parent);

} // namespace lifegame::view
