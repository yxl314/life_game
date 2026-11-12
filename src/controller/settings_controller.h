#pragma once

/**
 * @file settings_controller.h
 * @brief 设置对话框控制器。
 */

#include <QObject>

class QMainWindow;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class MessageSink;
}

namespace lifegame::controller
{

class SessionController;

/// @brief 设置对话框的输入与应用。
class SettingsController : public QObject
{
    Q_OBJECT

public:
    /// @brief 绑定模型、会话设置持有者与对话框父窗口。
    /// @param model 用于读取/应用的模型。
    /// @param session 当前设置的会话持有者。
    /// @param window 对话框父窗口。
    /// @param messages 状态栏输出端口。
    /// @param parent QObject 父对象。
    SettingsController(model::LifeModel* model,
                       SessionController* session,
                       QMainWindow* window,
                       view::MessageSink* messages,
                       QObject* parent = nullptr);

public slots:
    /// @brief 打开设置对话框，确认后保存。
    ///
    /// 跳转进行中拒绝打开；取消对话框不影响设置与已保存的会话。
    void openSettings();

private:
    model::LifeModel* m_model = nullptr;     ///< 用于读取/应用的模型。
    SessionController* m_session = nullptr;  ///< 当前设置的会话持有者。
    QMainWindow* m_window = nullptr;         ///< 对话框父窗口。
    view::MessageSink* m_messages = nullptr; ///< 状态栏输出端口。
};

} // namespace lifegame::controller
