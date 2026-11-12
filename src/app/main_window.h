#pragma once

/**
 * @file main_window.h
 * @brief 应用组装根与外壳接线。
 */

#include <QMainWindow>

#include <memory>

class QAction;
class QActionGroup;

namespace lifegame::controller
{
class BoardController;
class BoardPresenter;
class ConfigurationController;
class ControllerSettingsBridge;
class FileController;
class JumpController;
class SessionController;
class SettingsController;
class SimulationController;
} // namespace controller

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class SettingsStore;
class RleFileService;
class ImageFileService;
}

namespace lifegame::view
{
class BoardView;
class ControlPanel;
class PatternCatalog;
class RuleCatalog;
class StatusBarView;
class ConfirmSink;
class MessageBoxConfirmSink;
} // namespace view

namespace lifegame::app
{

/**
 * @brief 组装根与应用外壳。
 *
 * 创建模型、视图、目录、服务与控制器，接好菜单和工具栏；这里不实现游戏
 * 规则，也不实现持久化。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// @brief 构造整个对象图，并恢复已持久化的会话。
    ///
    /// 视图、目录和控制器都以本窗口为 QObject 父对象。恢复没有拿到棋盘时
    /// 载入内置示例图案。各控制器由 ~MainWindow() 显式删除，先于 QObject
    /// 子对象表销毁它们指向的模型与视图。
    /// @param parent 父控件。
    /// @param confirm 可选的确认端口；为空时使用 MessageBoxConfirmSink。
    explicit MainWindow(QWidget* parent = nullptr, view::ConfirmSink* confirm = nullptr);
    /// @brief 先删除控制器，再让模型与视图随子对象表销毁。
    ~MainWindow() override;

private:
    /// @brief 摆放被动棋盘视图与可滚动的侧边面板。
    void setupCentralWidget();
    /// @brief 创建状态栏及其常驻快捷键提示。
    void setupStatusBar();
    /// @brief 创建文件/编辑/模拟/规则/视图/帮助菜单及其动作。
    void setupMenus();
    /// @brief 用 setupMenus() 建好的动作创建主工具栏。
    void setupToolBar();

protected:
    /// @brief 把单独的空格键转成开始/暂停。
    ///
    /// 当获得焦点的控件自己处理空格（按钮、下拉框、数值框、滑块、编辑框）
    /// 时，按键交给 QMainWindow 默认处理。
    /// @param event 正在派发的按键事件。
    void keyPressEvent(QKeyEvent* event) override;

    model::LifeModel* m_model = nullptr;              ///< 棋盘状态，由本窗口持有。
    view::BoardView* m_view = nullptr;                ///< 被动棋盘画布。
    view::ControlPanel* m_panel = nullptr;            ///< 被动侧边面板。
    view::RuleCatalog* m_ruleCatalog = nullptr;       ///< 规则菜单用的规则预设。
    view::PatternCatalog* m_patternCatalog = nullptr; ///< 面板用的图案名称。
    view::StatusBarView* m_statusBar = nullptr;       ///< 状态消息出口。

    controller::BoardController* m_boardController = nullptr; ///< 棋盘输入与命令。
    controller::BoardPresenter* m_boardPresenter = nullptr;   ///< 模型到视图的刷新。
    controller::SimulationController* m_simulationController = nullptr; ///< 时钟与速度。
    controller::JumpController* m_jumpController = nullptr;   ///< 回退与快进。
    controller::FileController* m_fileController = nullptr;   ///< RLE 与 PNG 文件。
    controller::ConfigurationController* m_configurationController = nullptr; ///< 规则与边界。
    controller::SessionController* m_sessionController = nullptr; ///< 恢复与保存。
    controller::SettingsController* m_settingsController = nullptr; ///< 设置对话框。
    std::unique_ptr<controller::ControllerSettingsBridge> m_settingsBridge; ///< 设置端口。

    std::unique_ptr<service::SettingsStore> m_settingsStore;   ///< QSettings 存储。
    std::unique_ptr<service::RleFileService> m_rleService;     ///< RLE 文件读写。
    std::unique_ptr<service::ImageFileService> m_imageService; ///< PNG 输出。
    std::unique_ptr<view::MessageBoxConfirmSink> m_defaultConfirm; ///< 未注入时使用的确认端口。
    view::ConfirmSink* m_confirm = nullptr;                    ///< 实际使用的确认端口。

    QAction* m_openAction = nullptr;     ///< 文件 > 打开。
    QAction* m_saveAction = nullptr;     ///< 文件 > 保存。
    QAction* m_exportAction = nullptr;   ///< 文件 > 导出 PNG。
    QAction* m_playAction = nullptr;     ///< 模拟 > 开始/暂停。
    QAction* m_stepAction = nullptr;     ///< 模拟 > 单步。
    QAction* m_randomAction = nullptr;   ///< 编辑 > 随机填充。
    QAction* m_clearAction = nullptr;    ///< 编辑 > 清空棋盘。
    QAction* m_zoomInAction = nullptr;   ///< 编辑 > 放大。
    QAction* m_zoomOutAction = nullptr;  ///< 编辑 > 缩小。
    QAction* m_resetZoomAction = nullptr; ///< 编辑 > 重置缩放。
    QAction* m_gridAction = nullptr;     ///< 视图 > 显示网格线。
    QActionGroup* m_ruleGroup = nullptr; ///< 规则预设的互斥动作组。
};

} // namespace lifegame::app
