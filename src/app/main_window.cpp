#include "app/main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSlider>
#include <QSpinBox>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include <algorithm>

#include "controller/board_controller.h"
#include "controller/board_presenter.h"
#include "controller/configuration_controller.h"
#include "controller/controller_settings_bridge.h"
#include "controller/file_controller.h"
#include "controller/jump_controller.h"
#include "controller/session_controller.h"
#include "controller/settings_controller.h"
#include "controller/simulation_controller.h"
#include "model/life_model.h"
#include "service/image_file_service.h"
#include "service/rle_file_service.h"
#include "service/settings_store.h"
#include "view/board_view.h"
#include "view/control_panel.h"
#include "view/dialogs.h"
#include "view/jump_progress_dialog.h"
#include "view/message_box_confirm_sink.h"
#include "view/pattern_catalog.h"
#include "view/rule_catalog.h"
#include "view/status_bar_view.h"

namespace lifegame::app
{

MainWindow::MainWindow(QWidget* parent, view::ConfirmSink* confirm)
    : QMainWindow(parent)
{
    setWindowTitle(tr("生命游戏"));
    setMinimumSize(900, 620);

    if (QScreen* screen = QGuiApplication::primaryScreen())
    {
        const QRect available = screen->availableGeometry();
        resize(std::min(1180, available.width() - 60), std::min(820, available.height() - 60));
    }
    else
    {
        resize(1180, 820);
    }

    m_model = new model::LifeModel(this);
    m_view = new view::BoardView(this);
    m_panel = new view::ControlPanel(this);
    setupCentralWidget();

    m_ruleCatalog = new view::RuleCatalog(this);
    m_patternCatalog = new view::PatternCatalog(this);

    m_panel->setRuleCatalog(m_ruleCatalog);

    setupStatusBar();

    auto settings = std::make_unique<QSettings>();
    m_settingsStore = std::make_unique<service::SettingsStore>(std::move(settings));
    m_rleService = std::make_unique<service::RleFileService>();
    m_imageService = std::make_unique<service::ImageFileService>();
    if (confirm != nullptr)
    {
        m_confirm = confirm;
    }
    else
    {
        m_defaultConfirm = std::make_unique<view::MessageBoxConfirmSink>(this);
        m_confirm = m_defaultConfirm.get();
    }

    m_boardPresenter = new controller::BoardPresenter(m_model, m_view, m_panel, m_view, this);
    m_boardController = new controller::BoardController(
        m_model, m_view, m_panel, m_patternCatalog, m_statusBar, m_confirm, this);
    m_simulationController = new controller::SimulationController(m_model, m_panel, m_statusBar, this);
    m_jumpController = new controller::JumpController(
        m_model,
        m_panel,
        m_statusBar,
        [this] { return std::make_unique<view::JumpProgressDialog>(this); },
        this);
    m_fileController = new controller::FileController(
        m_model, m_view, m_statusBar, this, m_rleService.get(), m_imageService.get(), this);
    m_configurationController = new controller::ConfigurationController(
        m_model, m_panel, m_ruleCatalog, m_statusBar, this);
    m_settingsBridge = std::make_unique<controller::ControllerSettingsBridge>(
        m_model, m_simulationController, m_jumpController, m_boardController);
    m_sessionController = new controller::SessionController(m_model,
                                                            m_settingsBridge.get(),
                                                            m_settingsStore.get(),
                                                            this,
                                                            m_statusBar,
                                                            this);
    m_settingsController =
        new controller::SettingsController(m_model, m_sessionController, this, m_statusBar, this);

    setupMenus();
    setupToolBar();

    m_simulationController->setPlayPauseAction(m_playAction);
    m_boardPresenter->setGenerationRate(m_simulationController->speed());
    connect(m_simulationController,
            &controller::SimulationController::speedChanged,
            m_boardPresenter,
            &controller::BoardPresenter::setGenerationRate);
    m_boardController->setGridAction(m_gridAction);
    m_configurationController->setRuleActionGroup(m_ruleGroup);

    const bool restored = m_sessionController->restore();
    if (!restored)
    {
        m_boardController->loadDemo();
    }

    // Restore may have changed rule/boundary/grid/speed; resync the shell.
    m_simulationController->setPlayPauseAction(m_playAction);
    m_boardPresenter->setGenerationRate(m_simulationController->speed());
    m_boardController->setGridAction(m_gridAction);
    m_configurationController->setRuleActionGroup(m_ruleGroup);

    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] { (void)m_sessionController->save(); });
    m_statusBar->showMessage(tr("就绪 · 左键绘制，中键拖动平移，滚轮缩放"), 0);
}

MainWindow::~MainWindow()
{
    // Controllers hold model/view pointers. Destroy them explicitly before
    // the QObject child list deletes the model/views, so a future controller
    // destructor can never observe a half-destroyed model.
    delete m_settingsController;
    m_settingsController = nullptr;
    delete m_sessionController;
    m_sessionController = nullptr;
    delete m_boardPresenter;
    m_boardPresenter = nullptr;
    delete m_configurationController;
    m_configurationController = nullptr;
    delete m_fileController;
    m_fileController = nullptr;
    delete m_jumpController;
    m_jumpController = nullptr;
    delete m_simulationController;
    m_simulationController = nullptr;
    delete m_boardController;
    m_boardController = nullptr;
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier && !event->isAutoRepeat())
    {
        QWidget* focus = QApplication::focusWidget();
        const bool interactive = qobject_cast<QAbstractButton*>(focus) != nullptr ||
                                 qobject_cast<QComboBox*>(focus) != nullptr ||
                                 qobject_cast<QSpinBox*>(focus) != nullptr ||
                                 qobject_cast<QSlider*>(focus) != nullptr ||
                                 qobject_cast<QLineEdit*>(focus) != nullptr;
        if (!interactive)
        {
            m_playAction->trigger();
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::setupCentralWidget()
{
    auto* central = new QWidget(this);
    central->setObjectName(QStringLiteral("Central"));
    auto* layout = new QHBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_view, 1);

    auto* sideScroll = new QScrollArea(this);
    sideScroll->setObjectName(QStringLiteral("SideScroll"));
    sideScroll->setWidgetResizable(true);
    sideScroll->setFrameShape(QFrame::NoFrame);
    sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    sideScroll->setWidget(m_panel);
    sideScroll->setFixedWidth(302);
    sideScroll->viewport()->setAutoFillBackground(false);
    layout->addWidget(sideScroll, 0);
    setCentralWidget(central);
}

void MainWindow::setupStatusBar()
{
    statusBar()->setSizeGripEnabled(true);
    statusBar()->showMessage(tr("就绪"));
    m_statusBar = new view::StatusBarView(statusBar(), this);

    auto* hint = new QLabel(tr("空格 开始/暂停 · F6 单步 · 中键平移 / 滚轮缩放"), this);
    hint->setObjectName(QStringLiteral("StatusHint"));
    statusBar()->addPermanentWidget(hint);
}

void MainWindow::setupMenus()
{
    auto* fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    m_openAction = fileMenu->addAction(QIcon(QStringLiteral(":/icons/folder-open.svg")), tr("打开 RLE(&O)..."));
    m_openAction->setShortcut(QKeySequence::Open);
    m_openAction->setIconText(tr("打开"));
    m_openAction->setStatusTip(tr("从 RLE 文件载入图案或棋盘"));
    connect(m_openAction, &QAction::triggered, m_fileController, &controller::FileController::openRle);

    m_saveAction = fileMenu->addAction(QIcon(QStringLiteral(":/icons/save.svg")), tr("保存为 RLE(&S)..."));
    m_saveAction->setShortcut(QKeySequence::Save);
    m_saveAction->setIconText(tr("保存"));
    m_saveAction->setStatusTip(tr("把当前棋盘保存为 RLE 文件"));
    connect(m_saveAction, &QAction::triggered, m_fileController, &controller::FileController::saveRle);

    m_exportAction = fileMenu->addAction(QIcon(QStringLiteral(":/icons/image-down.svg")), tr("导出 PNG(&E)..."));
    m_exportAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
    m_exportAction->setIconText(tr("导出 PNG"));
    m_exportAction->setStatusTip(tr("把当前棋盘导出为图片"));
    connect(m_exportAction, &QAction::triggered, m_fileController, &controller::FileController::exportImage);

    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(QIcon(QStringLiteral(":/icons/power.svg")), tr("退出(&Q)"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);

    auto* editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    QAction* demoAction =
        editMenu->addAction(QIcon(QStringLiteral(":/icons/sparkles.svg")), tr("载入示例图案(&D)"));
    demoAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+D")));
    demoAction->setStatusTip(tr("载入高斯帕滑翔机炮或滑翔机"));
    connect(demoAction, &QAction::triggered, m_boardController, &controller::BoardController::loadDemo);

    m_randomAction = editMenu->addAction(QIcon(QStringLiteral(":/icons/dices.svg")), tr("随机填充(&R)"));
    m_randomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    m_randomAction->setIconText(tr("随机填充"));
    m_randomAction->setStatusTip(tr("按侧边面板的密度随机撒点"));
    connect(m_randomAction, &QAction::triggered, m_boardController, &controller::BoardController::randomizeBoard);

    m_clearAction = editMenu->addAction(QIcon(QStringLiteral(":/icons/trash-2.svg")), tr("清空棋盘(&C)"));
    m_clearAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Delete")));
    m_clearAction->setIconText(tr("清空"));
    m_clearAction->setStatusTip(tr("清空所有细胞"));
    connect(m_clearAction, &QAction::triggered, m_boardController, &controller::BoardController::clearBoard);

    editMenu->addSeparator();
    m_zoomInAction = editMenu->addAction(QIcon(QStringLiteral(":/icons/zoom-in.svg")), tr("放大(&I)"));
    m_zoomInAction->setShortcuts({QKeySequence(QStringLiteral("Ctrl+=")), QKeySequence(QStringLiteral("Ctrl++"))});
    connect(m_zoomInAction, &QAction::triggered, m_boardController, &controller::BoardController::zoomIn);

    m_zoomOutAction = editMenu->addAction(QIcon(QStringLiteral(":/icons/zoom-out.svg")), tr("缩小(&O)"));
    m_zoomOutAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+-")));
    connect(m_zoomOutAction, &QAction::triggered, m_boardController, &controller::BoardController::zoomOut);

    m_resetZoomAction = editMenu->addAction(QIcon(QStringLiteral(":/icons/rotate-ccw.svg")), tr("重置缩放(&0)"));
    m_resetZoomAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(m_resetZoomAction, &QAction::triggered, m_boardController, &controller::BoardController::resetZoom);

    editMenu->addSeparator();
    QAction* settingsAction = editMenu->addAction(QIcon(QStringLiteral(":/icons/settings.svg")), tr("设置(&S)…"));
    settingsAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+,")));
    settingsAction->setStatusTip(tr("调整跳转粒度、尺寸粒度与回退快照间隔"));
    connect(settingsAction, &QAction::triggered, m_settingsController, &controller::SettingsController::openSettings);

    auto* simMenu = menuBar()->addMenu(tr("模拟(&S)"));
    m_playAction = simMenu->addAction(QIcon(QStringLiteral(":/icons/play.svg")), tr("开始(&P)"));
    m_playAction->setIconText(tr("开始"));
    m_playAction->setStatusTip(tr("开始或暂停模拟"));
    connect(m_playAction, &QAction::triggered, m_simulationController, &controller::SimulationController::toggleRunning);

    m_stepAction = simMenu->addAction(QIcon(QStringLiteral(":/icons/step-forward.svg")), tr("单步(&N)"));
    m_stepAction->setShortcut(QKeySequence(Qt::Key_F6));
    m_stepAction->setIconText(tr("单步"));
    m_stepAction->setStatusTip(tr("暂停并演化一代"));
    connect(m_stepAction, &QAction::triggered, m_simulationController, &controller::SimulationController::stepOnce);

    simMenu->addSeparator();
    QAction* rewindAction = simMenu->addAction(QIcon(QStringLiteral(":/icons/rewind.svg")), tr("回退指定代数(&B)"));
    rewindAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Left")));
    rewindAction->setStatusTip(tr("按侧边面板的代数回退，不逐代刷新界面"));
    connect(rewindAction, &QAction::triggered, m_jumpController, &controller::JumpController::rewind);

    QAction* forwardAction =
        simMenu->addAction(QIcon(QStringLiteral(":/icons/fast-forward.svg")), tr("快进指定代数(&F)"));
    forwardAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+Right")));
    forwardAction->setStatusTip(tr("按侧边面板的代数快进，不逐代刷新界面"));
    connect(forwardAction, &QAction::triggered, m_jumpController, &controller::JumpController::fastForward);

    auto* ruleMenu = menuBar()->addMenu(tr("规则(&R)"));
    m_ruleGroup = new QActionGroup(this);
    m_ruleGroup->setExclusive(true);
    for (const view::RulePresetView& preset : m_ruleCatalog->presets())
    {
        QAction* action = ruleMenu->addAction(preset.name);
        action->setCheckable(true);
        action->setData(preset.notation);
        action->setStatusTip(tr("%1：B/S 记号 %2").arg(preset.name, preset.notation));
        m_ruleGroup->addAction(action);
        const QString notation = preset.notation;
        connect(action, &QAction::triggered, this, [this, notation] {
            m_configurationController->setRuleByNotation(notation);
        });
    }

    auto* viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    m_gridAction = viewMenu->addAction(tr("显示网格线(&G)"));
    m_gridAction->setCheckable(true);
    m_gridAction->setChecked(m_boardController->gridVisible());
    m_gridAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+G")));
    connect(m_gridAction, &QAction::toggled, m_boardController, &controller::BoardController::setGridVisible);

    QAction* fullscreenAction =
        viewMenu->addAction(QIcon(QStringLiteral(":/icons/maximize.svg")), tr("全屏(&F)"));
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullscreenAction, &QAction::triggered, this, [this] {
        if (isFullScreen())
        {
            showNormal();
        }
        else
        {
            showFullScreen();
        }
    });

    auto* helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    QAction* controls = helpMenu->addAction(QIcon(QStringLiteral(":/icons/circle-help.svg")), tr("操作说明(&C)"));
    controls->setShortcut(QKeySequence::HelpContents);
    connect(controls, &QAction::triggered, this, [this] { view::showControlsHelp(this); });

    QAction* about = helpMenu->addAction(QIcon(QStringLiteral(":/icons/info.svg")), tr("关于生命游戏(&A)"));
    connect(about, &QAction::triggered, this, [this] {
        view::showAbout(this, QCoreApplication::applicationVersion());
    });

    QAction* aboutQt = helpMenu->addAction(QIcon(QStringLiteral(":/icons/info.svg")), tr("关于 Qt(&Q)"));
    connect(aboutQt, &QAction::triggered, this, [this] { view::showAboutQt(this); });
}

void MainWindow::setupToolBar()
{
    auto* toolbar = addToolBar(tr("主工具栏"));
    toolbar->setObjectName(QStringLiteral("MainToolBar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setIconSize(QSize(18, 18));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    toolbar->addAction(m_playAction);
    toolbar->addAction(m_stepAction);
    toolbar->addSeparator();
    toolbar->addAction(m_randomAction);
    toolbar->addAction(m_clearAction);
    toolbar->addSeparator();
    toolbar->addAction(m_openAction);
    toolbar->addAction(m_saveAction);
    toolbar->addAction(m_exportAction);
    toolbar->addSeparator();
    toolbar->addAction(m_zoomOutAction);
    toolbar->addAction(m_zoomInAction);
    toolbar->addAction(m_resetZoomAction);

    for (QAction* action : {m_zoomOutAction, m_zoomInAction, m_resetZoomAction})
    {
        if (auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(action)))
        {
            button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        }
    }
}

} // namespace lifegame::app
