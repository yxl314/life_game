#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLabel>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>
#include <functional>
#include <memory>
#include <vector>

#include "controller/board_controller.h"
#include "controller/board_presenter.h"
#include "controller/controller_settings_bridge.h"
#include "controller/file_controller.h"
#include "controller/jump_controller.h"
#include "controller/line_raster.h"
#include "controller/session_controller.h"
#include "controller/simulation_controller.h"
#include "domain/pattern_library.h"
#include "domain/rle.h"
#include "model/life_model.h"

#define VERIFY_OK(expression) QVERIFY(lifegame::model::succeeded(expression))
#define VERIFY_FAIL(expression) QVERIFY(lifegame::model::failed(expression))
#include "service/image_file_service.h"
#include "service/rle_file_service.h"
#include "service/session_state.h"
#include "service/settings_store.h"
#include "view/board_view.h"
#include "view/control_panel.h"
#include "view/interfaces.h"
#include "view/pattern_catalog.h"

namespace
{

class FakeMessages : public lifegame::view::MessageSink
{
public:
    void showMessage(const QString& text, int timeoutMs) override
    {
        Q_UNUSED(timeoutMs)
        last = text;
        ++count;
    }

    QString last;
    int count = 0;
};

struct JumpState
{
    bool shown = false;
    bool closed = false;
    int progress = 0;
    lifegame::shared::JumpInfo info;
    std::function<void()> cancel;
};

class FakeJumpProgress : public lifegame::view::JumpProgressView
{
public:
    explicit FakeJumpProgress(JumpState* state)
        : m_state(state)
    {
    }

    void show(const lifegame::shared::JumpInfo& info) override
    {
        m_state->shown = true;
        m_state->info = info;
    }

    void setProgress(int done) override
    {
        m_state->progress = done;
    }

    void close() override
    {
        m_state->closed = true;
    }

    void setCancelHandler(std::function<void()> handler) override
    {
        m_state->cancel = std::move(handler);
    }

private:
    JumpState* m_state = nullptr;
};

struct SessionHarness
{
    lifegame::model::LifeModel model;
    lifegame::view::ControlPanel panel;
    lifegame::view::BoardView view;
    lifegame::view::PatternCatalog catalog;
    FakeMessages messages;
    lifegame::controller::BoardController board;
    lifegame::controller::SimulationController simulation;
    lifegame::controller::JumpController jump;
    lifegame::controller::ControllerSettingsBridge bridge;
    lifegame::service::SettingsStore store;
    lifegame::controller::SessionController session;

    explicit SessionHarness(const QString& path)
        : model(),
          panel(),
          view(),
          catalog(),
          messages(),
          board(&model, &view, &panel, &catalog, &messages, nullptr),
          simulation(&model, &panel, &messages, nullptr),
          jump(&model, &panel, &messages, {}, nullptr),
          bridge(&model, &simulation, &jump, &board),
          store(std::make_unique<QSettings>(path, QSettings::IniFormat)),
          session(&model, &bridge, &store, nullptr, &messages, nullptr)
    {
    }
};

void place(lifegame::model::LifeModel* model, const std::string& id, int row, int col)
{
    const auto* pattern = lifegame::domain::findBuiltinPattern(id);
    QVERIFY(pattern != nullptr);
    VERIFY_OK(model->placePattern(*pattern, row, col));
}

} // namespace

class ControllerTests : public QObject
{
    Q_OBJECT

private slots:
    void lineRaster();
    void boardControllerEditsAndStamps();
    void simulationControllerDrivesModel();
    void jumpControllerReportsProgressAndCancel();
    void settingsStoreRoundTrip();
    void rleFileService();
    void imageFileService();
    void fileControllerPaths();
    void settingsCorruptData();
    void sessionControllerRoundTrip();
    void sessionRestoreReportsInvalidRule();
};

void ControllerTests::lineRaster()
{
    using lifegame::controller::CellPosition;
    using lifegame::controller::rasterLine;

    const auto horizontal = rasterLine(2, 2, 2, 5);
    QCOMPARE(horizontal.size(), std::size_t{4});
    QCOMPARE(horizontal.front().row, 2);
    QCOMPARE(horizontal.front().col, 2);
    QCOMPARE(horizontal.back().col, 5);

    const auto diagonal = rasterLine(0, 0, 4, 4);
    QCOMPARE(diagonal.size(), std::size_t{5});
    for (std::size_t i = 0; i < diagonal.size(); ++i)
    {
        QCOMPARE(diagonal[i].row, diagonal[i].col);
    }

    const auto reverse = rasterLine(5, 5, 2, 2);
    QCOMPARE(reverse.front().row, 5);
    QCOMPARE(reverse.back().row, 2);
    QCOMPARE(reverse.size(), std::size_t{4});

    QCOMPARE(rasterLine(1, 1, 1, 1).size(), std::size_t{1});
}

void ControllerTests::boardControllerEditsAndStamps()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    lifegame::view::BoardView view;
    lifegame::view::ControlPanel panel;
    lifegame::view::PatternCatalog catalog;
    FakeMessages messages;

    lifegame::controller::BoardPresenter presenter(&model, &view, &panel, &view, nullptr);
    lifegame::controller::BoardController controller(&model, &view, &panel, &catalog, &messages, nullptr);

    const auto statLabels = panel.findChildren<QLabel*>(QStringLiteral("StatValue"));
    QCOMPARE(statLabels.size(), 3);
    QVERIFY(!view.renderBoardImage().isNull()); // the controller pushed the board

    emit view.cellPressed(5, 5);
    QVERIFY(model.board().cell(5, 5));
    QCOMPARE(model.stats().population, 1);
    QCOMPARE(statLabels[1]->text(), QStringLiteral("1")); // controller pushed stats to the view

    emit view.cellDragged(5, 10);
    for (int col = 5; col <= 10; ++col)
    {
        QVERIFY(model.board().cell(5, col));
    }
    QCOMPARE(model.stats().population, 6);
    emit view.cellReleased(5, 10);

    controller.clearBoard();
    QCOMPARE(model.stats().population, 0);

    emit panel.patternChanged(QStringLiteral("glider"));
    emit view.cellPressed(10, 10);
    QCOMPARE(model.stats().population, 5);

    controller.setGridVisible(false);
    QCOMPARE(view.gridVisible(), false);
    QCOMPARE(panel.isGridVisible(), false);
    QCOMPARE(controller.gridVisible(), false);

    // The side panel must actually be wired, not just emit signals.
    emit panel.resizeRequested(30, 25);
    QCOMPARE(model.width(), 30);
    QCOMPARE(model.height(), 25);
    emit panel.clearRequested();
    QCOMPARE(model.stats().population, 0);
    panel.setRandomDensity(5);
    emit panel.randomizeRequested(90); // must honor the signal argument, not the spin value
    QVERIFY(model.stats().population > 600); // 30x25 board, ~90% alive
    emit panel.gridVisibilityChanged(true);
    QCOMPARE(view.gridVisible(), true);
    QCOMPARE(panel.isGridVisible(), true);

    controller.loadDemo();
    QVERIFY(model.stats().population > 0);
}

void ControllerTests::simulationControllerDrivesModel()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    VERIFY_OK(model.setCell(5, 5, true));
    lifegame::view::ControlPanel panel;
    FakeMessages messages;
    lifegame::controller::SimulationController controller(&model, &panel, &messages, nullptr);

    controller.stepOnce();
    QCOMPARE(model.stats().generation, 1);
    QCOMPARE(model.isRunning(), false);

    // The single cell died in the step; give the simulation something to run.
    VERIFY_OK(model.setCell(6, 6, true));
    controller.toggleRunning();
    QCOMPARE(model.isRunning(), true);
    controller.setSpeedExternal(200);
    QTRY_VERIFY(model.stats().generation > 1);
    controller.toggleRunning();
    QCOMPARE(model.isRunning(), false);

    model.clear();
    controller.toggleRunning();
    QCOMPARE(model.isRunning(), false);
    QVERIFY(messages.last.contains(QStringLiteral("棋盘为空")));

    controller.setSpeedExternal(42);
    QCOMPARE(controller.speed(), 42);
    QCOMPARE(panel.speed(), 42);

    VERIFY_OK(model.setCell(7, 7, true));
    controller.setSpeedExternal(0);
    QCOMPARE(panel.speed(), 0);
    controller.toggleRunning();
    QCOMPARE(model.isRunning(), false);
    QVERIFY(messages.last.contains(QStringLiteral("速度为 0")));

    controller.setSpeedExternal(10);
    controller.toggleRunning();
    QCOMPARE(model.isRunning(), true);
    controller.setSpeedExternal(0);
    QCOMPARE(model.isRunning(), false);
}

void ControllerTests::jumpControllerReportsProgressAndCancel()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(60, 40));
    place(&model, "glider", 20, 20);

    lifegame::view::ControlPanel panel;
    FakeMessages messages;
    JumpState state;

    auto factory = [&state]() -> std::unique_ptr<lifegame::view::JumpProgressView> {
        return std::make_unique<FakeJumpProgress>(&state);
    };
    lifegame::controller::JumpController controller(&model, &panel, &messages, factory, nullptr);

    controller.jumpBy(120);
    QTRY_VERIFY(!model.isJumping());
    QCOMPARE(model.stats().generation, 120);
    QVERIFY(state.shown);
    QVERIFY(state.closed);

    // The panel rewind signal carries a positive count; the controller must negate it.
    emit panel.rewindRequested(20);
    QTRY_VERIFY(!model.isJumping());
    QCOMPARE(model.stats().generation, 100);
    emit panel.fastForwardRequested(20);
    QTRY_VERIFY(!model.isJumping());
    QCOMPARE(model.stats().generation, 120);

    controller.jumpBy(100000);
    QVERIFY(state.shown);
    QVERIFY(state.cancel);
    state.cancel();
    QTRY_VERIFY(!model.isJumping());
    QCOMPARE(model.stats().generation, 120);
    QVERIFY(state.closed);
}

void ControllerTests::settingsStoreRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.path() + QStringLiteral("/settings.ini");

    lifegame::service::SessionState state;
    state.settings.jumpStep = 500;
    state.settings.sizeStep = 10;
    state.settings.snapshotInterval = 250;
    state.settings.density = 55;
    state.settings.speed = 42;
    state.settings.gridVisible = false;
    state.boardRle = QStringLiteral("x = 3, y = 2, rule = B3/S23\nbo$2bo$3o!\n");
    state.ruleNotation = QStringLiteral("B36/S23");
    state.boundary = lifegame::domain::Boundary::Wrap;
    state.generation = 77;
    state.windowGeometry = QByteArrayLiteral("geometry");

    {
        auto settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
        lifegame::service::SettingsStore store(std::move(settings));
        QCOMPARE(store.save(state), lifegame::service::SettingsError::None);
    }

    auto settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
    lifegame::service::SettingsStore store(std::move(settings));
    const lifegame::service::SettingsLoadResult loaded = store.load();
    QCOMPARE(loaded.error, lifegame::service::SettingsError::None);

    const lifegame::service::SessionState& value = loaded.state;
    QCOMPARE(value.settings.jumpStep, 500);
    QCOMPARE(value.settings.sizeStep, 10);
    QCOMPARE(value.settings.snapshotInterval, 250);
    QCOMPARE(value.settings.density, 55);
    QCOMPARE(value.settings.speed, 42);
    QCOMPARE(value.settings.gridVisible, false);
    QCOMPARE(value.boardRle, state.boardRle);
    QCOMPARE(value.ruleNotation, state.ruleNotation);
    QCOMPARE(value.boundary, lifegame::domain::Boundary::Wrap);
    QCOMPARE(value.generation, 77);
    QCOMPARE(value.windowGeometry, state.windowGeometry);
}

void ControllerTests::rleFileService()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.path() + QStringLiteral("/glider.rle");
    const QString text = QStringLiteral("#N Glider\nx = 3, y = 3, rule = B3/S23\nbob$2bo$3o!\n");

    lifegame::service::RleFileService service;
    QString detail;
    QCOMPARE(service.write(path, text, &detail), lifegame::service::FileError::None);
    QVERIFY(detail.isEmpty());

    const lifegame::service::RleReadResult result = service.read(path);
    QCOMPARE(result.error, lifegame::service::FileError::None);
    QCOMPARE(result.decodeError.code, lifegame::domain::RleError::None);
    QVERIFY(result.document.has_value());
    if (result.document)
    {
        QCOMPARE(result.document->pattern.population(), 5);
        QCOMPARE(result.document->rule, std::string("B3/S23"));
    }

    const lifegame::service::RleReadResult missing = service.read(directory.path() + QStringLiteral("/nope.rle"));
    QCOMPARE(missing.error, lifegame::service::FileError::OpenFailed);
    QVERIFY(!missing.document.has_value());

    const QString brokenPath = directory.path() + QStringLiteral("/broken.rle");
    QCOMPARE(service.write(brokenPath, QStringLiteral("x = 3, y = 3\nbo?!"), &detail),
             lifegame::service::FileError::None);
    const lifegame::service::RleReadResult broken = service.read(brokenPath);
    QCOMPARE(broken.error, lifegame::service::FileError::DecodeFailed);
    QCOMPARE(broken.decodeError.code, lifegame::domain::RleError::InvalidCharacter);

    const QString hugePath = directory.path() + QStringLiteral("/huge.rle");
    QFile huge(hugePath);
    QVERIFY(huge.open(QIODevice::WriteOnly));
    QCOMPARE(huge.write(QByteArray(8 * 1024 * 1024 + 1, 'x')), qint64(8 * 1024 * 1024 + 1));
    huge.close();
    QCOMPARE(service.read(hugePath).error, lifegame::service::FileError::TooLarge);

    QCOMPARE(service.write(directory.path(), text, &detail), lifegame::service::FileError::OpenFailed);
}

void ControllerTests::imageFileService()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    QImage image(32, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::green);

    lifegame::service::ImageFileService service;
    const QString path = directory.path() + QStringLiteral("/frame.png");
    QString detail;
    QCOMPARE(service.savePng(image, path, &detail), lifegame::service::FileError::None);
    QVERIFY(QFileInfo::exists(path));

    QCOMPARE(service.savePng(QImage(), path, &detail), lifegame::service::FileError::NullImage);
    QCOMPARE(service.savePng(image, directory.path() + QStringLiteral("/missing/frame.png"), &detail),
             lifegame::service::FileError::EncodeFailed);
}

void ControllerTests::fileControllerPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    lifegame::view::BoardView view;
    lifegame::view::BoardFrame frame;
    frame.board = &model.board();
    frame.population = model.stats().population;
    view.applyFrame(frame);
    FakeMessages messages;
    lifegame::service::RleFileService rleService;
    lifegame::service::ImageFileService imageService;
    lifegame::controller::FileController controller(
        &model, &view, &messages, nullptr, &rleService, &imageService, nullptr);

    const QString rlePath = directory.path() + QStringLiteral("/glider.rle");
    QFile rle(rlePath);
    QVERIFY(rle.open(QIODevice::WriteOnly | QIODevice::Truncate));
    rle.write("x = 3, y = 3\nbob$2bo$3o!\n");
    rle.close();

    const lifegame::controller::FileController::OpenResult opened = controller.openPath(rlePath);
    QCOMPARE(opened.fileError, lifegame::service::FileError::None);
    QCOMPARE(opened.modelResult, lifegame::model::CommandResult::Success);
    QCOMPARE(model.stats().population, 5);

    const QString savePath = directory.path() + QStringLiteral("/saved.rle");
    QCOMPARE(controller.savePath(savePath), lifegame::service::FileError::None);
    QVERIFY(QFileInfo::exists(savePath));

    const QString imagePath = directory.path() + QStringLiteral("/board.png");
    QCOMPARE(controller.exportPath(imagePath), lifegame::service::FileError::None);
    QVERIFY(QFileInfo::exists(imagePath));

    const QString brokenPath = directory.path() + QStringLiteral("/broken.rle");
    QFile broken(brokenPath);
    QVERIFY(broken.open(QIODevice::WriteOnly | QIODevice::Truncate));
    broken.write("x = 3, y = 3\nbo?!");
    broken.close();
    QCOMPARE(controller.openPath(brokenPath).fileError, lifegame::service::FileError::DecodeFailed);
    QCOMPARE(controller.openPath(directory.path() + QStringLiteral("/missing.rle")).fileError,
             lifegame::service::FileError::OpenFailed);
}

void ControllerTests::settingsCorruptData()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.path() + QStringLiteral("/corrupt.ini");

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("[simulation]\ndensity=not-an-int\n");
    file.close();

    auto settings = std::make_unique<QSettings>(path, QSettings::IniFormat);
    lifegame::service::SettingsStore store(std::move(settings));
    const lifegame::service::SettingsLoadResult result = store.load();
    QCOMPARE(result.error, lifegame::service::SettingsError::CorruptData);
}

void ControllerTests::sessionControllerRoundTrip()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.path() + QStringLiteral("/session.ini");

    {
        SessionHarness harness(path);
        QVERIFY(!harness.session.restore()); // nothing persisted yet

        VERIFY_OK(harness.model.setCell(5, 5, true));
        harness.board.setRandomDensity(55);
        harness.board.setGridVisible(false);
        harness.simulation.setSpeedExternal(42);
        harness.jump.setJumpStep(250);
        QCOMPARE(harness.board.randomDensity(), 55);
        QCOMPARE(harness.panel.randomDensity(), 55);
        QCOMPARE(harness.simulation.speed(), 42);
        QCOMPARE(harness.panel.speed(), 42);
        QCOMPARE(harness.jump.jumpStep(), 250);
        QCOMPARE(harness.panel.jumpGenerations(), 250);
        harness.session.save();
    }

    {
        SessionHarness harness(path);
        QVERIFY(harness.session.restore());

        QCOMPARE(harness.model.width(), 250);
        QCOMPARE(harness.model.height(), 200);
        QVERIFY(harness.model.board().cell(5, 5));
        QCOMPARE(harness.board.randomDensity(), 55);
        QCOMPARE(harness.panel.randomDensity(), 55);
        QCOMPARE(harness.simulation.speed(), 42);
        QCOMPARE(harness.panel.speed(), 42);
        QCOMPARE(harness.board.gridVisible(), false);
    }
}

void ControllerTests::sessionRestoreReportsInvalidRule()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.path() + QStringLiteral("/invalid-rule.ini");

    {
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("simulation/rule"), QStringLiteral("not-a-rule"));
        settings.sync();
    }

    SessionHarness harness(path);
    QVERIFY(!harness.session.restore());
    QCOMPARE(harness.model.rule(), lifegame::domain::Rule());
    QVERIFY(harness.messages.count > 0);
}

QTEST_MAIN(ControllerTests)

#include "controller_tests.moc"
