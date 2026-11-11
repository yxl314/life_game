#pragma once

/**
 * @file life_model.h
 * @brief Observable application model.
 */

#include <QFutureWatcher>
#include <QObject>

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "domain/grid.h"
#include "domain/history.h"
#include "domain/packed_grid.h"
#include "domain/pattern.h"
#include "domain/rle.h"
#include "domain/rule.h"
#include "shared/app_settings.h"
#include "model/command_result.h"
#include "shared/jump_info.h"
#include "shared/life_stats.h"
#include "model/jump_runner.h"
#include "model/simulation_engine.h"
#include "model/stall_reason.h"

Q_DECLARE_METATYPE(lifegame::shared::LifeStats)
Q_DECLARE_METATYPE(lifegame::shared::JumpInfo)

namespace lifegame::model
{

/**
 * @brief Observable application model for Conway's Game of Life.
 *
 * Layered MVC contract:
 *  - Model knows nothing about views, services or controllers.
 *  - State changes are broadcast as signals; Controllers subscribe and push
 *    the resulting state into passive Views.
 *  - No file I/O, dialogs, QSettings or localized strings live here.
 *
 * All public methods run on the UI thread. The asynchronous step pipeline is
 * delegated to SimulationEngine; the asynchronous jump worker only reads a
 * snapshot and reports progress back through a queued invocation.
 */
class LifeModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Create a model with a default board.
     * @param parent QObject parent.
     */
    explicit LifeModel(QObject* parent = nullptr);
    ~LifeModel() override;

    /// @return Current board (read-only).
    const domain::Grid& board() const;

    /// @return Current board width.
    int width() const;

    /// @return Current board height.
    int height() const;

    /// @return Current generation/population/density snapshot.
    shared::LifeStats stats() const;

    /// @return Current Life-like rule.
    const domain::Rule& rule() const;

    /// @return Current boundary policy.
    domain::Boundary boundary() const;

    /// @return True while the simulation clock is running.
    bool isRunning() const;

    /// @return True while a background jump is in progress.
    bool isJumping() const;

    /// @return Oldest generation reachable from retained snapshots.
    int oldestGeneration() const;

    /// @return Snapshot interval in generations.
    int snapshotInterval() const;

    /// @param title Optional RLE title.
    /// @return RLE text for the current board/rule (no file I/O).
    std::string toRle(std::string_view title = {}) const;

    // ---- commands (controllers only) ----
    /// @param row Row index.
    /// @param col Column index.
    /// @param alive New state.
    /// @return Success, NoChange, OutOfBounds or Busy.
    CommandResult setCell(int row, int col, bool alive);

    /// @param row Row index.
    /// @param col Column index.
    /// @return Success, NoChange, OutOfBounds or Busy.
    CommandResult toggleCell(int row, int col);

    /// @param cells Coordinates to paint.
    /// @param alive Desired state.
    /// @return Success, NoChange, OutOfBounds or Busy.
    CommandResult setCells(const std::vector<std::pair<int, int>>& cells, bool alive);

    /// @param pattern Pattern to place.
    /// @param centerRow Center row.
    /// @param centerCol Center column.
    /// @return Success, Clipped, Busy, InvalidPattern or TooLarge.
    CommandResult placePattern(const domain::Pattern& pattern, int centerRow, int centerCol);

    /// @return Success after clearing the board.
    CommandResult clear();

    /// @param densityPercent Alive percentage.
    /// @return Success, adjusted, or failure code.
    CommandResult randomize(int densityPercent);

    /// @param width Requested width.
    /// @param height Requested height.
    /// @return Success, Adjusted, or failure code.
    CommandResult resizeBoard(int width, int height);

    /// @param rule New rule.
    /// @return Success or NoChange.
    CommandResult setRule(const domain::Rule& rule);

    /// @param notation B/S notation.
    /// @return Success, NoChange or InvalidRule.
    CommandResult setRuleByNotation(std::string_view notation);

    /// @param boundary New boundary policy.
    /// @return Success or NoChange.
    CommandResult setBoundary(domain::Boundary boundary);

    /// @param generations Snapshot interval.
    /// @return Success, NoChange or Busy.
    CommandResult setSnapshotInterval(int generations);

    /// @param generation Generation to set (restore-only).
    /// @return Success or Busy.
    CommandResult setGeneration(int generation);

    /// @param document Decoded RLE document.
    /// @return Success or a typed failure.
    CommandResult loadDocument(const domain::DecodedDocument& document);

    /// @return Success after one synchronous generation.
    CommandResult step();

    /// @return Busy, NotRunning or Success for an async step request.
    CommandResult requestStep();

    /// @param running Desired running state.
    /// @return Success, NoChange, EmptyBoard or Busy.
    CommandResult setRunning(bool running);

    /// @return Result of toggling the running state.
    CommandResult toggleRunning();

    /// @param targetGeneration Target generation.
    /// @return Success, NoHistory, InvalidTarget, Busy or OutOfMemory.
    CommandResult startJump(int targetGeneration);

    /// @brief Request cooperative cancellation of the active jump.
    void cancelJump();

signals:
    /// @brief Emitted when cells/population/layout changed.
    void boardChanged();

    /// @brief Emitted when the board dimensions changed.
    /// @param width New width.
    /// @param height New height.
    void boardResized(int width, int height);

    /// @brief Emitted when generation/population/density changed.
    /// @param stats New statistics.
    void statusChanged(const lifegame::shared::LifeStats& stats);

    /// @brief Emitted when the running state changed.
    /// @param running New state.
    void runningChanged(bool running);

    /// @brief Emitted when rule/boundary/snapshot interval changed.
    void configurationChanged();

    /// @brief Emitted when the simulation stopped because it stalled.
    /// @param reason Stall reason.
    void simulationStalled(lifegame::model::StallReason reason);

    /// @brief Emitted when a background calculation failed.
    /// @param reason Failure code.
    void stepFailed(lifegame::model::CommandResult reason);

    /// @brief Emitted when a rewind/fast-forward started.
    /// @param info Jump parameters.
    void jumpStarted(const lifegame::shared::JumpInfo& info);

    /// @brief Emitted periodically during a jump.
    /// @param done Replayed generations.
    void jumpProgress(int done);

    /// @brief Emitted when a jump completed.
    /// @param generation Final generation.
    void jumpFinished(int generation);

    /// @brief Emitted when a jump was cancelled.
    void jumpCancelled();

    /// @brief Emitted when a jump failed.
    /// @param reason Failure code.
    void jumpFailed(lifegame::model::CommandResult reason);

private slots:
    void onStepReady(quint64 requestId);
    void onStepFailed(lifegame::model::CommandResult reason);
    void onJumpFinished();
    void reportJumpProgress(int done);

private:
    void pauseForEdit();
    void resetHistory();
    void recordSnapshot();
    void invalidateHistoryFrom(int generation);
    shared::LifeStats computeStats() const;
    void notifyBoardChanged();
    void notifyStatusChanged();
    void applyStep(domain::Grid&& next, const domain::GridStats& gridStats);
    CommandResult replaceBoard(std::unique_ptr<domain::Grid> board, int generation);
    bool canMutate() const;

    domain::Grid m_board;
    domain::Rule m_rule;
    domain::Boundary m_boundary = domain::Boundary::Dead;
    int m_generation = 0;
    int m_population = 0;
    bool m_running = false;

    domain::History m_history;
    SimulationEngine m_engine;
    quint64 m_pendingStepId = 0;

    QFutureWatcher<JumpOutcome> m_jumpWatcher;
    std::shared_ptr<std::atomic_bool> m_jumpCancel;
    bool m_jumping = false;
};

} // namespace lifegame::model
