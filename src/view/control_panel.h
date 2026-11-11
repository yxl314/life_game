#pragma once

/**
 * @file control_panel.h
 * @brief Passive side panel and user-intent signals.
 */

#include <QString>
#include <QVector>
#include <QWidget>

#include "domain/boundary.h"
#include "shared/app_settings.h"
#include "shared/life_stats.h"
#include "view/view_state.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSlider;
class QSpinBox;

namespace lifegame::view
{

class RuleCatalog;

/// @brief One entry in the pattern combo box.
struct PatternOption
{
    QString text; ///< Localized label.
    QString id;   ///< Stable pattern ID; empty means the brush.
};

/**
 * @brief Passive side panel: controllers push display state, panel emits intents.
 *
 * It has no model/controller dependency.
 */
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ControlPanel(QWidget* parent = nullptr);

    /// @param catalog Presentation catalog; may be null.
    void setRuleCatalog(const RuleCatalog* catalog);

    /// @brief Atomically apply one complete panel state.
    /// @param state Stats, size, rule, boundary, oldest and running state.
    void applyState(const PanelState& state);

    /// @param stats Statistics to display.
    void setStats(const shared::LifeStats& stats);

    /// @param running Running state.
    void setRunning(bool running);

    /// @param width Board width.
    /// @param height Board height.
    void setBoardSize(int width, int height);

    /// @param notation Canonical rule notation.
    void setRuleNotation(const QString& notation);

    /// @param boundary Boundary policy to display.
    void setBoundary(domain::Boundary boundary);

    /// @param generation Oldest rewindable generation.
    void setOldestGeneration(int generation);

    /// @param options Pattern choices.
    void setPatternOptions(const QVector<PatternOption>& options);

    /// @param text Description shown below the pattern selector.
    void setPatternDescription(const QString& text);

    /// @param speed Generations per second (0 = pause).
    void setSpeed(int speed);

    /// @return Current generations per second.
    int speed() const;

    /// @return Current random-fill percentage.
    int randomDensity() const;

    /// @param density Random-fill percentage.
    void setRandomDensity(int density);

    /// @return True when grid lines are visible.
    bool isGridVisible() const;

    /// @param visible Whether grid lines are shown.
    void setGridVisible(bool visible);

    /// @param step Jump amount step.
    void setJumpStep(int step);

    /// @return Current jump amount.
    int jumpGenerations() const;

    /// @param step Board-size spinner step.
    void setSizeStep(int step);

signals:
    void playPauseRequested();                 ///< Play/pause button clicked.
    void stepRequested();                      ///< Single-step requested.
    void clearRequested();                     ///< Clear-board requested.
    void randomizeRequested(int density);      ///< Randomize with a percentage.
    void randomDensityChanged(int density);    ///< Density spin box changed.
    void jumpStepChanged(int generations);     ///< Jump amount spin box changed.
    void speedChanged(int speed);              ///< Speed slider changed.
    void rewindRequested(int generations);     ///< Rewind button clicked.
    void fastForwardRequested(int generations);///< Fast-forward button clicked.
    void patternChanged(const QString& patternId); ///< Pattern selection changed.
    void ruleChanged(int index);               ///< Rule preset changed.
    void boundaryChanged(domain::Boundary boundary); ///< Boundary mode changed.
    void resizeRequested(int width, int height);///< Apply size requested.
    void gridVisibilityChanged(bool visible);  ///< Grid checkbox changed.

private:
    void setupUi();
    void syncRuleSelection();
    void syncBoundarySelection();
    void updateJumpState();
    void updateSpeedLabel();

    const RuleCatalog* m_ruleCatalog = nullptr;
    QString m_ruleNotation = QStringLiteral("B3/S23");
    domain::Boundary m_boundary = domain::Boundary::Dead;

    QLabel* m_generationValue = nullptr;
    QLabel* m_populationValue = nullptr;
    QLabel* m_densityValue = nullptr;

    QPushButton* m_playButton = nullptr;
    QPushButton* m_stepButton = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_randomButton = nullptr;
    QSpinBox* m_densitySpin = nullptr;

    QSlider* m_speedSlider = nullptr;
    QLabel* m_speedValue = nullptr;
    int m_speed = shared::limits::kDefaultSpeed;

    QSpinBox* m_jumpSpin = nullptr;
    int m_jumpStep = 100;
    QPushButton* m_rewindButton = nullptr;
    QPushButton* m_forwardButton = nullptr;
    QPushButton* m_applySizeButton = nullptr;

    QComboBox* m_patternCombo = nullptr;
    QLabel* m_patternText = nullptr;

    QComboBox* m_ruleCombo = nullptr;
    QComboBox* m_boundaryCombo = nullptr;

    QSpinBox* m_widthSpin = nullptr;
    QSpinBox* m_heightSpin = nullptr;
    QCheckBox* m_gridCheck = nullptr;

    int m_currentGeneration = 0;
    int m_population = 0;
    double m_density = -1.0;
    int m_oldestGeneration = 0;
    int m_panelWidth = 0;
    int m_panelHeight = 0;
    bool m_running = false;
    bool m_syncing = false;
};

} // namespace lifegame::view
