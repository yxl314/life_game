#pragma once

/**
 * @file session_controller.h
 * @brief 会话恢复/保存与设置生命周期的协调者。
 */

#include <QObject>

#include "shared/app_settings.h"
#include "service/settings_error.h"

#include <memory>

class QMainWindow;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class SettingsStore;
}

namespace lifegame::view
{
class MessageSink;
}

namespace lifegame::controller
{

class SessionService;
class SessionSettingsPort;

/**
 * @brief 应用/会话协调者：启动时恢复，退出时持久化。
 *
 * 持有当前运行归一化后的设置快照，并通过状态栏端口报告恢复/保存问题。
 * 文件读写由 SessionService 负责；具体控制器只经 SessionSettingsPort 访问。
 */
class SessionController : public QObject
{
    Q_OBJECT

public:
    /// @brief 绑定模型、设置端口与持久化存储。
    /// @param model 需要恢复/保存的模型。
    /// @param settingsPort 指向控制器侧输入设置的窄适配器。
    /// @param store 持久化存储。
    /// @param window 窗口几何来源。
    /// @param messages 状态栏输出端口。
    /// @param parent QObject 父对象。
    SessionController(model::LifeModel* model,
                      SessionSettingsPort* settingsPort,
                      service::SettingsStore* store,
                      QMainWindow* window,
                      view::MessageSink* messages,
                      QObject* parent = nullptr);
    /// @brief 释放持久化服务；析构本身不写盘。
    ~SessionController() override;

    /// @brief 读取已保存的会话，并应用到模型与外壳。
    ///
    /// 棋盘缺失、无法解析或被模型拒绝时回退到默认棋盘尺寸；成功恢复或回退
    /// 会经状态栏端口报出，没有保存记录时静默使用默认值。
    /// @return 恢复了持久化棋盘时返回 true。
    bool restore();

    /// @brief 采集当前状态并写入存储。
    /// @return 保存结果码。
    service::SettingsError save();

    /// @brief 最近一次应用/归一化的设置。
    /// @return 设置快照。
    const shared::AppSettings& settings() const;

    /// @brief 把控制器侧当前的输入状态采集进 settings()。
    void refreshSettings();

    /// @brief 归一化并应用设置到模型与控制器。
    /// @param values 待归一化并应用的设置。
    void applySettings(const shared::AppSettings& values);

private:
    model::LifeModel* m_model = nullptr;           ///< 需要恢复/保存的模型。
    SessionSettingsPort* m_settingsPort = nullptr; ///< 控制器设置适配器，可为空。
    std::unique_ptr<SessionService> m_service;     ///< 与 UI 无关的持久化服务。
    QMainWindow* m_window = nullptr;               ///< 窗口几何来源，可为空。
    view::MessageSink* m_messages = nullptr;       ///< 状态栏输出端口。
    shared::AppSettings m_settings;                ///< 当前会话归一化后的设置。
};

} // namespace lifegame::controller
