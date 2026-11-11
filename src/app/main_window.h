#pragma once

/**
 * @file main_window.h
 * @brief Application composition root and shell wiring.
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
 * @brief Composition root and application shell.
 *
 * Creates the model, views, catalogs, services and controllers; wires menus
 * and the toolbar; nothing here implements game rules or persistence.
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
     * @param parent Parent widget.
     * @param confirm Optional confirmation sink; null uses MessageBoxConfirmSink.
     */
    explicit MainWindow(QWidget* parent = nullptr, view::ConfirmSink* confirm = nullptr);
    ~MainWindow() override;

private:
    void setupCentralWidget();
    void setupStatusBar();
    void setupMenus();
    void setupToolBar();

protected:
    void keyPressEvent(QKeyEvent* event) override;

    model::LifeModel* m_model = nullptr;
    view::BoardView* m_view = nullptr;
    view::ControlPanel* m_panel = nullptr;
    view::RuleCatalog* m_ruleCatalog = nullptr;
    view::PatternCatalog* m_patternCatalog = nullptr;
    view::StatusBarView* m_statusBar = nullptr;

    controller::BoardController* m_boardController = nullptr;
    controller::BoardPresenter* m_boardPresenter = nullptr;
    controller::SimulationController* m_simulationController = nullptr;
    controller::JumpController* m_jumpController = nullptr;
    controller::FileController* m_fileController = nullptr;
    controller::ConfigurationController* m_configurationController = nullptr;
    controller::SessionController* m_sessionController = nullptr;
    controller::SettingsController* m_settingsController = nullptr;
    std::unique_ptr<controller::ControllerSettingsBridge> m_settingsBridge;

    std::unique_ptr<service::SettingsStore> m_settingsStore;
    std::unique_ptr<service::RleFileService> m_rleService;
    std::unique_ptr<service::ImageFileService> m_imageService;
    std::unique_ptr<view::MessageBoxConfirmSink> m_defaultConfirm;
    view::ConfirmSink* m_confirm = nullptr;

    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_exportAction = nullptr;
    QAction* m_playAction = nullptr;
    QAction* m_stepAction = nullptr;
    QAction* m_randomAction = nullptr;
    QAction* m_clearAction = nullptr;
    QAction* m_zoomInAction = nullptr;
    QAction* m_zoomOutAction = nullptr;
    QAction* m_resetZoomAction = nullptr;
    QAction* m_gridAction = nullptr;
    QActionGroup* m_ruleGroup = nullptr;
};

} // namespace lifegame::app
