#pragma once

/**
 * @file simulation_engine.h
 * @brief Asynchronous one-generation computation engine.
 */

#include <QFutureWatcher>
#include <QObject>

#include <cstdint>
#include <memory>

#include "domain/boundary.h"
#include "domain/grid.h"
#include "domain/rule.h"
#include "model/command_result.h"

namespace lifegame::model
{

/**
 * @brief Asynchronous one-generation computation.
 *
 * The engine owns the snapshot and the output buffer. `request()` returns a
 * monotonically increasing id; `stepReady(id)` is emitted on the UI thread when
 * the computation finishes. `discardPending()` makes the in-flight result
 * irrelevant (the worker still finishes, but no signal is emitted and a new
 * request is allowed once it returns).
 *
 * The engine owns the one-generation QtConcurrent pipeline; jump replay also
 * uses QtConcurrent, in LifeModel, but is kept separate from this class. This keeps
 * `LifeModel` focused on state and invariants.
 */
class SimulationEngine : public QObject
{
    Q_OBJECT

public:
    explicit SimulationEngine(QObject* parent = nullptr);
    ~SimulationEngine() override;

    /// @return True while a computation is in flight.
    bool isBusy() const;

    /**
     * @brief Start one asynchronous generation.
     * @param board Read-only current board.
     * @param rule Rule to apply.
     * @param boundary Boundary policy.
     * @param requestId Output request id; must not be null.
     * @return Success, Busy, OutOfBounds or OutOfMemory.
     */
    CommandResult request(const domain::Grid& board,
                          const domain::Rule& rule,
                          domain::Boundary boundary,
                          quint64* requestId);

    /// @brief Invalidate the in-flight request; the worker is not interrupted.
    void discardPending();

    /// @brief Move the finished generation out of the engine.
    /// @param stats Optional output statistics collected during computation.
    /// @return Finished board; only call after stepReady().
    domain::Grid takeResult(domain::GridStats* stats = nullptr);

signals:
    /// @brief Emitted on the UI thread when a generation is ready.
    /// @param requestId Id returned by request().
    void stepReady(quint64 requestId);

    /// @brief Emitted when the computation failed or was incomplete.
    /// @param reason Failure code.
    void stepFailed(lifegame::model::CommandResult reason);

private slots:
    void onFinished();

private:
    QFutureWatcher<domain::GridStats> m_watcher;
    domain::Grid m_output;
    domain::GridStats m_stats;
    std::shared_ptr<const domain::Grid> m_snapshot;
    quint64 m_activeId = 0;
    quint64 m_nextId = 1;
    bool m_busy = false;
    int m_expectedTasks = 0;
};

} // namespace lifegame::model
