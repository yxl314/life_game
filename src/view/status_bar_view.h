#pragma once

/**
 * @file status_bar_view.h
 * @brief MessageSink 的状态栏实现。
 */

#include <QObject>

#include "view/interfaces.h"

class QStatusBar;

namespace lifegame::view
{

/// @brief 把控制器使用的窄 MessageSink 适配到窗口状态栏。
class StatusBarView : public QObject, public MessageSink
{
    Q_OBJECT

public:
    /// @brief 构造适配器。
    /// @param bar 要适配的状态栏。
    /// @param parent QObject 父对象。
    explicit StatusBarView(QStatusBar* bar, QObject* parent = nullptr);

    /// @brief 在状态栏显示一条消息。
    /// @param text 消息文本。
    /// @param timeoutMs 自动隐藏的毫秒数；0 表示常驻。
    void showMessage(const QString& text, int timeoutMs) override;

private:
    QStatusBar* m_bar = nullptr; ///< 被适配的状态栏。
};

} // namespace lifegame::view
