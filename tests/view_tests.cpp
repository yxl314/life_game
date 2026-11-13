#include <QApplication>
#include <QLabel>
#include <QSignalSpy>
#include <QSlider>
#include <QtTest>

#include <algorithm>
#include <vector>

#include "domain/grid.h"
#include "domain/pattern_library.h"
#include "domain/rule.h"
#include "view/board_view.h"
#include "view/jump_progress_dialog.h"
#include "view/control_panel.h"
#include "view/pattern_catalog.h"
#include "view/rule_catalog.h"
#include "view/settings_dialog.h"
#include "view/speed_scale.h"

namespace
{

std::vector<QLabel*> statLabels(QWidget& panel)
{
    std::vector<QLabel*> labels;
    for (QLabel* label : panel.findChildren<QLabel*>(QStringLiteral("StatValue")))
    {
        labels.push_back(label);
    }
    std::sort(labels.begin(), labels.end(), [&panel](QLabel* left, QLabel* right) {
        return left->mapTo(&panel, QPoint(0, 0)).x() < right->mapTo(&panel, QPoint(0, 0)).x();
    });
    return labels;
}

void pushBoard(lifegame::view::BoardView& view, const lifegame::domain::Grid& grid)
{
    lifegame::view::BoardFrame frame;
    frame.board = &grid;
    frame.population = grid.population();
    view.applyFrame(frame);
}

} // namespace

class ViewTests : public QObject
{
    Q_OBJECT

private slots:
    void speedScale();
    void speedSliderDoesNotWriteBack();
    void controlPanelDisplaysPushedState();
    void boardViewRendersPushedStateAndCapsExport();
    void catalogs();
    void settingsDialogRoundTrip();
    void progressDialogProgrammaticCloseDoesNotCancel();
    void largeBoardRendersAndExports();
    void zoomedBoardPaintsVisibleRegion();
    void resizeResetsZoom();
};

void ViewTests::speedScale()
{
    QCOMPARE(lifegame::view::speedForSliderPosition(0), 0);
    QCOMPARE(lifegame::view::speedForSliderPosition(1), 1);
    QCOMPARE(lifegame::view::speedForSliderPosition(100), 120);
    QCOMPARE(lifegame::view::sliderPositionForSpeed(0), 0);
    QCOMPARE(lifegame::view::sliderPositionForSpeed(1), 1);
    QCOMPARE(lifegame::view::sliderPositionForSpeed(120), 100);
}

void ViewTests::speedSliderDoesNotWriteBack()
{
    lifegame::view::ControlPanel panel;
    panel.setSpeed(10);

    auto* slider = panel.findChild<QSlider*>();
    QVERIFY(slider != nullptr);
    QCOMPARE(slider->value(), lifegame::view::sliderPositionForSpeed(10));

    QSignalSpy speedSpy(&panel, &lifegame::view::ControlPanel::speedChanged);
    slider->setValue(9);
    QCOMPARE(speedSpy.count(), 1);
    QCOMPARE(speedSpy.first().first().toInt(), lifegame::view::speedForSliderPosition(9));

    // The panel must not snap the thumb back while the user is dragging.
    QCOMPARE(slider->value(), 9);
    QCOMPARE(panel.speed(), lifegame::view::speedForSliderPosition(9));

    panel.setSpeed(0);
    QCOMPARE(panel.speed(), 0);
    QCOMPARE(slider->value(), 0);
}

void ViewTests::controlPanelDisplaysPushedState()
{
    lifegame::view::ControlPanel panel;
    lifegame::view::RuleCatalog catalog;
    panel.setRuleCatalog(&catalog);
    panel.setBoardSize(20, 20);

    const auto labels = statLabels(panel);
    QCOMPARE(labels.size(), 3);
    panel.setStats({0, 0, 0.0});
    QCOMPARE(labels[0]->text(), QStringLiteral("0"));
    QCOMPARE(labels[1]->text(), QStringLiteral("0"));

    panel.setStats({0, 1, 0.25});
    QCOMPARE(labels[0]->text(), QStringLiteral("0"));
    QCOMPARE(labels[1]->text(), QStringLiteral("1"));
    QCOMPARE(labels[2]->text(), QStringLiteral("0.3%"));

    panel.setStats({1, 0, 0.0});
    QCOMPARE(labels[0]->text(), QStringLiteral("1"));
    QCOMPARE(labels[1]->text(), QStringLiteral("0"));
}

void ViewTests::boardViewRendersPushedStateAndCapsExport()
{
    lifegame::domain::Grid grid(20, 20);
    grid.setCell(5, 5, true);

    lifegame::view::BoardView view;
    pushBoard(view, grid);
    view.resize(640, 480);
    view.update();

    const QImage image = view.renderBoardImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);

    lifegame::domain::Grid largeGrid(400, 400);
    largeGrid.setCell(0, 0, true);
    pushBoard(view, largeGrid);
    const QImage large = view.renderBoardImage();
    QVERIFY(!large.isNull());
    QVERIFY(large.width() <= 4096);
    QVERIFY(large.height() <= 4096);
}

void ViewTests::catalogs()
{
    lifegame::view::RuleCatalog rules;
    const auto& builtinRules = lifegame::domain::builtinRulePresets();
    QCOMPARE(rules.presets().size(), builtinRules.size());
    QCOMPARE(rules.names().size(), static_cast<int>(builtinRules.size()));
    for (std::size_t i = 0; i < builtinRules.size(); ++i)
    {
        QCOMPARE(rules.notationAt(static_cast<int>(i)),
                 QString::fromStdString(builtinRules[i].rule.notation()));
    }
    QCOMPARE(rules.nameForNotation(QStringLiteral("B3/S23")), QStringLiteral("康威生命"));
    QVERIFY(rules.notationAt(0) == QStringLiteral("B3/S23"));
    QVERIFY(rules.notationAt(-1).isEmpty());

    lifegame::view::PatternCatalog patterns;
    const auto& builtinPatterns = lifegame::domain::builtinPatterns();
    QCOMPARE(patterns.all().size(), builtinPatterns.size());
    for (const lifegame::domain::Pattern& pattern : builtinPatterns)
    {
        QVERIFY2(patterns.find(pattern.id()) != nullptr, pattern.id().c_str());
    }
    const auto* glider = patterns.find("glider");
    QVERIFY(glider != nullptr);
    QCOMPARE(glider->name, QStringLiteral("滑翔机"));
    QVERIFY(patterns.displayLabel(builtinPatterns.front()).contains(QStringLiteral("滑翔机")));
}

void ViewTests::settingsDialogRoundTrip()
{
    lifegame::view::SettingsDialog dialog;
    lifegame::shared::AppSettings values;
    values.jumpStep = 500;
    values.sizeStep = 10;
    values.snapshotInterval = 250;
    values.density = 55;
    values.speed = 42;
    values.gridVisible = false;
    dialog.setValues(values);

    const lifegame::shared::AppSettings result = dialog.values();
    QCOMPARE(result.jumpStep, 500);
    QCOMPARE(result.sizeStep, 10);
    QCOMPARE(result.snapshotInterval, 250);
    // Fields the dialog does not edit must survive the round-trip.
    QCOMPARE(result.density, 55);
    QCOMPARE(result.speed, 42);
    QCOMPARE(result.gridVisible, false);
}

void ViewTests::progressDialogProgrammaticCloseDoesNotCancel()
{
    bool cancelled = false;
    QWidget parent;
    lifegame::view::JumpProgressDialog dialog(&parent);
    dialog.setCancelHandler([&cancelled] { cancelled = true; });

    lifegame::shared::JumpInfo info;
    info.fromGeneration = 0;
    info.toGeneration = 100;
    info.replayGenerations = 100;
    dialog.show(info);

    // Simulates JumpController::closeProgress() during shutdown/finish.
    dialog.close();
    QVERIFY(!cancelled);
}

void ViewTests::largeBoardRendersAndExports()
{
    lifegame::domain::Grid grid(1500, 1000);
    grid.setCell(0, 0, true);
    grid.setCell(1499, 999, true);
    grid.setCell(750, 500, true);

    lifegame::view::BoardView view;
    view.resize(1180, 820);
    pushBoard(view, grid);
    view.update();

    const QImage image = view.renderBoardImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);
    QVERIFY(image.width() <= 4096);
    QVERIFY(image.height() <= 4096);
}

void ViewTests::zoomedBoardPaintsVisibleRegion()
{
    lifegame::domain::Grid grid(1500, 1000);
    grid.setCell(750, 500, true);
    grid.setCell(0, 0, true);
    grid.setCell(999, 1499, true);

    lifegame::view::BoardView view;
    view.resize(400, 300);
    pushBoard(view, grid);
    view.show();
    view.zoomIn();
    view.zoomIn();

    const QImage grabbed = view.grab().toImage();
    QVERIFY(!grabbed.isNull());
    QVERIFY(grabbed.width() > 0);
    QVERIFY(grabbed.height() > 0);

    const QImage exported = view.renderBoardImage();
    QVERIFY(!exported.isNull());
    QVERIFY(exported.width() > 0);
    QVERIFY(exported.height() > 0);
}

void ViewTests::resizeResetsZoom()
{
    lifegame::domain::Grid small(10, 10);
    small.setCell(0, 0, true);

    lifegame::view::BoardView view;
    view.resize(400, 300);
    pushBoard(view, small);
    const double fitted = view.cellPixels();
    view.zoomIn();
    view.zoomIn();
    QVERIFY(view.cellPixels() != fitted);

    lifegame::domain::Grid larger(20, 20);
    larger.setCell(1, 1, true);
    pushBoard(view, larger);

    lifegame::view::BoardView fresh;
    fresh.resize(400, 300);
    pushBoard(fresh, larger);
    QCOMPARE(view.cellPixels(), fresh.cellPixels());
}

QTEST_MAIN(ViewTests)

#include "view_tests.moc"
