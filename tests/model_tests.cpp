#include <QSignalSpy>
#include <QtTest>

#include <new>
#include <string>

#include "domain/pattern_library.h"
#include "domain/rle.h"
#include "model/jump_runner.h"
#include "model/life_model.h"

#define VERIFY_OK(expression) QVERIFY(lifegame::model::succeeded(expression))
#define VERIFY_FAIL(expression) QVERIFY(lifegame::model::failed(expression))

namespace
{

bool waitFor(QSignalSpy& spy, int timeoutMs = 5000)
{
    return spy.count() > 0 || spy.wait(timeoutMs);
}

QString rle(const lifegame::model::LifeModel& model)
{
    return QString::fromStdString(model.toRle());
}

void place(lifegame::model::LifeModel* model, const std::string& id, int row, int col)
{
    const auto* pattern = lifegame::domain::findBuiltinPattern(id);
    QVERIFY(pattern != nullptr);
    VERIFY_OK(model->placePattern(*pattern, row, col));
}

} // namespace

class ModelTests : public QObject
{
    Q_OBJECT

private slots:
    void defaults();
    void setCellUpdatesStatsAndSignals();
    void editingPausesSimulation();
    void emptyBoardCannotStart();
    void synchronousEvolution();
    void randomizeProducesVariation();
    void resizeAndLoadDocument();
    void ruleAndBoundary();
    void asyncStep();
    void largeAsyncStep();
    void jumpForwardAndBackward();
    void jumpCanBeCancelled();
    void jumpStopsAtExtinctionGeneration();
    void loadWhileJumpingIsRejected();
    void snapshotInterval();
    void commandResults();
    void largeBoard();
};

void ModelTests::defaults()
{
    lifegame::model::LifeModel model;
    QCOMPARE(model.width(), lifegame::domain::kDefaultBoardWidth);
    QCOMPARE(model.height(), lifegame::domain::kDefaultBoardHeight);
    QCOMPARE(model.stats().generation, 0);
    QCOMPARE(model.stats().population, 0);
    QCOMPARE(model.stats().density, 0.0);
    QCOMPARE(model.rule().notation(), std::string("B3/S23"));
    QCOMPARE(model.boundary(), lifegame::domain::Boundary::Dead);
    QCOMPARE(model.isRunning(), false);
    QCOMPARE(model.isJumping(), false);
    QCOMPARE(model.oldestGeneration(), 0);
    QCOMPARE(model.snapshotInterval(), 100);
}

void ModelTests::setCellUpdatesStatsAndSignals()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));

    QSignalSpy boardSpy(&model, &lifegame::model::LifeModel::boardChanged);
    QSignalSpy statusSpy(&model, &lifegame::model::LifeModel::statusChanged);

    VERIFY_OK(model.setCell(10, 10, true));
    QCOMPARE(model.stats().population, 1);
    QCOMPARE(model.stats().density, 0.3); // 1 / 400 * 100 = 0.25 -> 0.3
    QCOMPARE(boardSpy.count(), 1);
    QCOMPARE(statusSpy.count(), 1);

    // Same value is a no-op and emits nothing.
    QCOMPARE(model.setCell(10, 10, true), lifegame::model::CommandResult::NoChange);
    QCOMPARE(boardSpy.count(), 1);

    // Out of bounds is rejected.
    QCOMPARE(model.setCell(-1, 0, true), lifegame::model::CommandResult::OutOfBounds);
    QCOMPARE(model.setCell(20, 20, true), lifegame::model::CommandResult::OutOfBounds);

    VERIFY_OK(model.toggleCell(10, 10)); // mutation happened: alive -> dead
    QCOMPARE(model.stats().population, 0);
    VERIFY_OK(model.toggleCell(10, 10)); // mutation happened: dead -> alive
    QCOMPARE(model.stats().population, 1);

    VERIFY_OK(model.clear());
    QCOMPARE(model.stats().population, 0);
    QCOMPARE(model.stats().generation, 0);
}

void ModelTests::editingPausesSimulation()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    VERIFY_OK(model.setCell(5, 5, true));
    VERIFY_OK(model.setRunning(true));
    QCOMPARE(model.isRunning(), true);

    QSignalSpy runningSpy(&model, &lifegame::model::LifeModel::runningChanged);
    VERIFY_OK(model.setCell(6, 6, true));
    QCOMPARE(model.isRunning(), false);
    QCOMPARE(runningSpy.count(), 1);
}

void ModelTests::emptyBoardCannotStart()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.clear());
    QSignalSpy stalledSpy(&model, &lifegame::model::LifeModel::simulationStalled);
    VERIFY_FAIL(model.setRunning(true));
    QCOMPARE(model.isRunning(), false);
    QCOMPARE(stalledSpy.count(), 1);
    if (!stalledSpy.isEmpty())
    {
        QCOMPARE(stalledSpy.first().first().value<lifegame::model::StallReason>(),
                 lifegame::model::StallReason::EmptyBoard);
    }
}

void ModelTests::synchronousEvolution()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    place(&model, "blinker", 10, 10);
    QCOMPARE(model.stats().population, 3);

    QSignalSpy boardSpy(&model, &lifegame::model::LifeModel::boardChanged);
    model.step();
    QCOMPARE(model.stats().generation, 1);
    QCOMPARE(boardSpy.count(), 1);
    QCOMPARE(model.board().cell(9, 10), true);
    QCOMPARE(model.board().cell(10, 10), true);
    QCOMPARE(model.board().cell(11, 10), true);
    QCOMPARE(model.stats().population, 3);

    model.step();
    QCOMPARE(model.stats().generation, 2);
    QCOMPARE(model.board().cell(10, 9), true);
    QCOMPARE(model.board().cell(10, 10), true);
    QCOMPARE(model.board().cell(10, 11), true);
}

void ModelTests::randomizeProducesVariation()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(30, 30));
    VERIFY_OK(model.randomize(35));
    const QString first = rle(model);
    QVERIFY(model.stats().population > 0);
    QCOMPARE(model.stats().generation, 0);

    VERIFY_OK(model.randomize(35));
    const QString second = rle(model);
    QVERIFY(first != second);
}

void ModelTests::resizeAndLoadDocument()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(35, 25));
    place(&model, "glider", 10, 10);
    const QString saved = rle(model);

    const auto decoded = lifegame::domain::decodeRle(saved.toStdString());
    QVERIFY(decoded.document.has_value());

    lifegame::model::LifeModel loaded;
    QSignalSpy resizedSpy(&loaded, &lifegame::model::LifeModel::boardResized);
    VERIFY_OK(loaded.loadDocument(*decoded.document));
    QCOMPARE(loaded.width(), 35);
    QCOMPARE(loaded.height(), 25);
    QCOMPARE(loaded.stats().population, model.stats().population);
    QCOMPARE(loaded.stats().generation, 0);
    QCOMPARE(resizedSpy.count(), 1);

    // Small RLE patterns are padded to the minimum board size.
    lifegame::model::LifeModel padded;
    const auto glider = lifegame::domain::decodeRle("x = 3, y = 3\nbob$2bo$3o!");
    QVERIFY(glider.document.has_value());
    VERIFY_OK(padded.loadDocument(*glider.document));
    QCOMPARE(padded.width(), lifegame::domain::kMinBoardExtent);
    QCOMPARE(padded.height(), lifegame::domain::kMinBoardExtent);
    QCOMPARE(padded.stats().population, 5);

    // Invalid rule in the document is rejected and leaves the model untouched.
    lifegame::domain::DecodedDocument invalid;
    invalid.pattern = lifegame::domain::Pattern::fromAscii("bad", {"O"});
    invalid.rule = "not-a-rule";
    VERIFY_FAIL(padded.loadDocument(invalid));
    QCOMPARE(padded.stats().population, 5);
}

void ModelTests::ruleAndBoundary()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    QSignalSpy configSpy(&model, &lifegame::model::LifeModel::configurationChanged);

    VERIFY_OK(model.setRuleByNotation("B36/S23"));
    QCOMPARE(model.rule().notation(), std::string("B36/S23"));
    QCOMPARE(configSpy.count(), 1);
    VERIFY_FAIL(model.setRuleByNotation("bad"));
    QCOMPARE(configSpy.count(), 1);

    VERIFY_OK(model.setBoundary(lifegame::domain::Boundary::Wrap));
    QCOMPARE(model.boundary(), lifegame::domain::Boundary::Wrap);
    QCOMPARE(configSpy.count(), 2);
    VERIFY_FAIL(model.setBoundary(lifegame::domain::Boundary::Wrap));
    QCOMPARE(configSpy.count(), 2);
}

void ModelTests::asyncStep()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));
    place(&model, "glider", 10, 10);
    VERIFY_OK(model.setRunning(true));

    QSignalSpy boardSpy(&model, &lifegame::model::LifeModel::boardChanged);
    model.requestStep();
    QVERIFY(waitFor(boardSpy));
    QTRY_COMPARE(model.stats().generation, 1);
    QCOMPARE(model.isRunning(), true);

    VERIFY_OK(model.setRunning(false));
    const int generation = model.stats().generation;
    model.requestStep();
    QTest::qWait(50);
    QCOMPARE(model.stats().generation, generation);
}

void ModelTests::largeAsyncStep()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(400, 400));
    VERIFY_OK(model.randomize(30));
    VERIFY_OK(model.setRunning(true));

    QSignalSpy boardSpy(&model, &lifegame::model::LifeModel::boardChanged);
    model.requestStep();
    QVERIFY(waitFor(boardSpy, 8000));
    QTRY_COMPARE(model.stats().generation, 1);
    QCOMPARE(model.width(), 400);
    QCOMPARE(model.height(), 400);
    QVERIFY(model.stats().population > 0);
}

void ModelTests::jumpForwardAndBackward()
{
    lifegame::model::LifeModel reference;
    VERIFY_OK(reference.resizeBoard(60, 40));
    place(&reference, "glider", 20, 20);
    for (int i = 0; i < 250; ++i)
    {
        reference.step();
    }

    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(60, 40));
    place(&model, "glider", 20, 20);
    VERIFY_OK(model.setRunning(true));

    QSignalSpy startedSpy(&model, &lifegame::model::LifeModel::jumpStarted);
    QSignalSpy finishedSpy(&model, &lifegame::model::LifeModel::jumpFinished);
    QSignalSpy progressSpy(&model, &lifegame::model::LifeModel::jumpProgress);

    VERIFY_OK(model.startJump(250));
    QCOMPARE(model.isRunning(), false);
    QCOMPARE(model.isJumping(), true);
    QVERIFY(waitFor(finishedSpy, 8000));
    QCOMPARE(model.isJumping(), false);
    QCOMPARE(model.stats().generation, 250);
    QCOMPARE(rle(model), rle(reference));
    QCOMPARE(startedSpy.count(), 1);
    QVERIFY(progressSpy.count() >= 1);

    finishedSpy.clear();
    VERIFY_OK(model.startJump(120));
    QVERIFY(waitFor(finishedSpy, 5000));
    QCOMPARE(model.stats().generation, 120);

    lifegame::model::LifeModel reference120;
    VERIFY_OK(reference120.resizeBoard(60, 40));
    place(&reference120, "glider", 20, 20);
    for (int i = 0; i < 120; ++i)
    {
        reference120.step();
    }
    QCOMPARE(rle(model), rle(reference120));

    finishedSpy.clear();
    VERIFY_OK(model.startJump(0));
    QVERIFY(waitFor(finishedSpy, 5000));
    QCOMPARE(model.stats().generation, 0);
    QCOMPARE(model.oldestGeneration(), 0);

    VERIFY_FAIL(model.startJump(-1));
    VERIFY_FAIL(model.startJump(0));
}

void ModelTests::jumpStopsAtExtinctionGeneration()
{
    std::shared_ptr<lifegame::domain::Grid> board(
        new (std::nothrow) lifegame::domain::Grid(1, 1));
    QVERIFY(board != nullptr);
    board->setCell(0, 0, true);

    lifegame::model::JumpRequest request;
    request.startBoard = board;
    request.startGeneration = 0;
    request.targetGeneration = 200000;
    request.totalGenerations = 200000;
    request.epoch = 0;
    request.interval = 100;

    const lifegame::model::JumpOutcome outcome = lifegame::model::runJump(request);
    VERIFY_OK(outcome.error);
    QVERIFY(outcome.board != nullptr);
    // 单格棋盘在第 1 代灭绝；检查点每 64 代触发一次，重放必须报告 64，
    // 而不是仍未到达的目标代数 200000。
    QCOMPARE(outcome.generation, 64);
}

void ModelTests::jumpCanBeCancelled()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(60, 40));
    place(&model, "glider", 20, 20);

    QSignalSpy cancelledSpy(&model, &lifegame::model::LifeModel::jumpCancelled);
    QSignalSpy finishedSpy(&model, &lifegame::model::LifeModel::jumpFinished);
    const QString before = rle(model);
    VERIFY_OK(model.startJump(200000));
    model.cancelJump();
    QVERIFY(waitFor(cancelledSpy, 8000));
    QCOMPARE(model.isJumping(), false);
    QCOMPARE(model.stats().generation, 0);
    QCOMPARE(rle(model), before);
    QCOMPARE(finishedSpy.count(), 0);
}

void ModelTests::loadWhileJumpingIsRejected()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(60, 40));
    place(&model, "glider", 20, 20);

    const auto document = lifegame::domain::decodeRle("x = 3, y = 3, rule = B36/S23\nbob$2bo$3o!");
    QVERIFY(document.document.has_value());

    QSignalSpy finishedSpy(&model, &lifegame::model::LifeModel::jumpFinished);
    VERIFY_OK(model.startJump(5000));
    QCOMPARE(model.rule().notation(), std::string("B3/S23"));
    VERIFY_FAIL(model.loadDocument(*document.document));
    QCOMPARE(model.rule().notation(), std::string("B3/S23"));
    QCOMPARE(model.stats().population, 5);

    QVERIFY(waitFor(finishedSpy, 8000));
    QCOMPARE(model.rule().notation(), std::string("B3/S23"));
    QCOMPARE(model.stats().generation, 5000);
}

void ModelTests::snapshotInterval()
{
    lifegame::model::LifeModel model;
    VERIFY_OK(model.resizeBoard(20, 20));

    VERIFY_OK(model.setSnapshotInterval(50));
    QCOMPARE(model.snapshotInterval(), 50);
    VERIFY_FAIL(model.setSnapshotInterval(50));
    VERIFY_OK(model.setSnapshotInterval(1)); // clamped to the minimum
    QCOMPARE(model.snapshotInterval(), lifegame::shared::limits::kSnapshotIntervalMin);
    VERIFY_OK(model.setSnapshotInterval(5000)); // clamped to the maximum
    QCOMPARE(model.snapshotInterval(), lifegame::shared::limits::kSnapshotIntervalMax);
}

void ModelTests::commandResults()
{
    using lifegame::model::CommandResult;

    lifegame::model::LifeModel model;
    QCOMPARE(model.resizeBoard(20, 20), CommandResult::Success);
    QCOMPARE(model.resizeBoard(0, 0), CommandResult::Adjusted);
    QCOMPARE(model.width(), 20);
    QCOMPARE(model.height(), 20);

    QCOMPARE(model.setRuleByNotation("bad"), CommandResult::InvalidRule);
    QCOMPARE(model.setRule(model.rule()), CommandResult::NoChange);
    QCOMPARE(model.setBoundary(model.boundary()), CommandResult::NoChange);
    QCOMPARE(model.setSnapshotInterval(model.snapshotInterval()), CommandResult::NoChange);
    QCOMPARE(model.setGeneration(model.stats().generation), CommandResult::NoChange);
    QCOMPARE(model.setRunning(false), CommandResult::NoChange);
    QCOMPARE(model.setRunning(true), CommandResult::EmptyBoard);
    QCOMPARE(model.requestStep(), CommandResult::NotRunning);

    QCOMPARE(model.randomize(30), CommandResult::Success);
    QCOMPARE(model.setRunning(true), CommandResult::Success);
    QCOMPARE(model.setRunning(false), CommandResult::Success);

    const auto* glider = lifegame::domain::findBuiltinPattern("glider");
    QVERIFY(glider != nullptr);
    // 先清空：3x3 图案放在 (0,0) 时只有中心格在盘内，而滑翔机的中心是空格，
    // 此时"没有任何格子会改变"，返回值是 NoChange 而不是 Clipped——后者意味
    // 着"放了，但被裁剪"。清空后才有真正会被裁剪的活细胞。
    QCOMPARE(model.clear(), CommandResult::Success);
    QCOMPARE(model.placePattern(*glider, 0, 0), CommandResult::Clipped);

    lifegame::domain::Pattern empty;
    QCOMPARE(model.placePattern(empty, 0, 0), CommandResult::InvalidPattern);

    lifegame::domain::DecodedDocument invalidRule;
    invalidRule.pattern = lifegame::domain::Pattern::fromAscii("one", {"O"});
    invalidRule.rule = "not-a-rule";
    QCOMPARE(model.loadDocument(invalidRule), CommandResult::InvalidRule);

    lifegame::domain::DecodedDocument tooLarge;
    tooLarge.pattern = lifegame::domain::Pattern::fromAscii("wide", {std::string(1501, 'O')});
    QCOMPARE(model.loadDocument(tooLarge), CommandResult::TooLarge);

    QCOMPARE(model.clear(), CommandResult::Success);
    VERIFY_OK(model.setCell(1, 1, true));
    QCOMPARE(model.resizeBoard(20, 20), CommandResult::Success);
    QCOMPARE(model.stats().population, 0); // same-size Apply deliberately resets the board

    QCOMPARE(model.clear(), CommandResult::Success);
    QCOMPARE(model.startJump(-1), CommandResult::InvalidTarget);
    QCOMPARE(model.startJump(model.stats().generation), CommandResult::InvalidTarget);
    QCOMPARE(model.setGeneration(10), CommandResult::Success);
    QCOMPARE(model.startJump(5), CommandResult::NoHistory);

    QCOMPARE(model.startJump(100), CommandResult::Success);
    QCOMPARE(model.step(), CommandResult::Busy);
    QCOMPARE(model.clear(), CommandResult::Busy);
    model.cancelJump();
    QTRY_VERIFY(!model.isJumping());
}

void ModelTests::largeBoard()
{
    lifegame::model::LifeModel model;
    QCOMPARE(model.resizeBoard(1500, 1000), lifegame::model::CommandResult::Success);
    QCOMPARE(model.width(), 1500);
    QCOMPARE(model.height(), 1000);

    QCOMPARE(model.randomize(30), lifegame::model::CommandResult::Success);
    QVERIFY(model.stats().population > 0);
    QCOMPARE(model.step(), lifegame::model::CommandResult::Success);
    QCOMPARE(model.stats().generation, 1);

    const std::string encoded = model.toRle("large-board");
    const auto decoded = lifegame::domain::decodeRle(encoded);
    QVERIFY(decoded.document.has_value());
    if (decoded.document)
    {
        QCOMPARE(decoded.document->pattern.width(), 1500);
        QCOMPARE(decoded.document->pattern.height(), 1000);
    }
}

QTEST_GUILESS_MAIN(ModelTests)

#include "model_tests.moc"
