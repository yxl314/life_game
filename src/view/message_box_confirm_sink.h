#pragma once

/**
 * @file message_box_confirm_sink.h
 * @brief QMessageBox 确认端口。
 */

#include "view/interfaces.h"

class QWidget;

namespace lifegame::view
{

/// @brief 用 QMessageBox 实现 ConfirmSink。
class MessageBoxConfirmSink : public ConfirmSink
{
public:
    /// @brief 构造确认端口。
    /// @param parent 对话框父窗口。
    explicit MessageBoxConfirmSink(QWidget* parent = nullptr);

    /// @brief 弹出确认对话框。
    /// @param title 对话框标题。
    /// @param text 确认正文。
    /// @return 用户接受时返回 true。
    bool confirm(const QString& title, const QString& text) override;

private:
    QWidget* m_parent = nullptr; ///< 对话框父窗口。
};

} // namespace lifegame::view
