#include "model/simulation_engine.h"

#include <stacklog/stacklog.h>

#include <QPair>
#include <QThreadPool>
#include <QVector>
#include <QtConcurrent>

#include <algorithm>
#include <utility>

namespace lifegame::model
{

namespace
{

constexpr int kParallelThreshold = 20000;
constexpr int kMinRowsPerChunk = 8;

/**
 * Functor passed to QtConcurrent::mapped.
 *
 * Qt 5 reads `result_type` from the functor, hence the explicit typedef
 * instead of a lambda.
 */
struct ComputeRowsTask
{
    using result_type = domain::GridStats;

    std::shared_ptr<const domain::Grid> snapshot;
    domain::Grid* output = nullptr;
    domain::Rule rule;
    domain::Boundary boundary = domain::Boundary::Dead;

    domain::GridStats operator()(const QPair<int, int>& range) const
    {
        domain::GridStats stats;
        domain::Grid::computeRows(*snapshot, output, rule, boundary, range.first, range.second, &stats);
        return stats;
    }
};

} // namespace

SimulationEngine::SimulationEngine(QObject* parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<domain::GridStats>::finished, this,
            &SimulationEngine::onFinished);
}

SimulationEngine::~SimulationEngine()
{
    disconnect(&m_watcher, nullptr, this, nullptr);
    if (m_watcher.isRunning())
    {
        m_watcher.waitForFinished();
    }
}

bool SimulationEngine::isBusy() const
{
    return m_busy;
}

CommandResult SimulationEngine::request(const domain::Grid& board,
                                         const domain::Rule& rule,
                                         domain::Boundary boundary,
                                         quint64* requestId)
{
    if (requestId == nullptr)
    {
        return CommandResult::InvalidArgument;
    }
    *requestId = 0;

    if (m_busy)
    {
        return CommandResult::Busy;
    }

    const int width = board.width();
    const int height = board.height();
    if (width <= 0 || height <= 0)
    {
        STACKLOG_WARN() << "step request rejected reason=out_of_bounds width=" << width
                        << " height=" << height;
        return CommandResult::OutOfBounds;
    }

    auto snapshot = std::shared_ptr<const domain::Grid>(new (std::nothrow) domain::Grid(board));
    if (!snapshot)
    {
        STACKLOG_ERROR() << "step request failed reason=out_of_memory width=" << width
                         << " height=" << height;
        return CommandResult::OutOfMemory;
    }
    m_snapshot = std::move(snapshot);
    m_output.resize(width, height);
    m_stats = domain::GridStats{};
    m_activeId = m_nextId++;
    m_busy = true;

    QVector<QPair<int, int>> ranges;
    int chunks = QThreadPool::globalInstance()->maxThreadCount();
    chunks = std::max(1, chunks);
    if (width * height < kParallelThreshold)
    {
        chunks = 1;
    }
    chunks = std::min(chunks, std::max(1, height / kMinRowsPerChunk));

    if (chunks <= 1)
    {
        ranges.append(qMakePair(0, height));
    }
    else
    {
        const int chunkRows = (height + chunks - 1) / chunks;
        for (int begin = 0; begin < height; begin += chunkRows)
        {
            ranges.append(qMakePair(begin, std::min(height, begin + chunkRows)));
        }
    }

    m_expectedTasks = static_cast<int>(ranges.size());
    const QFuture<domain::GridStats> future =
        QtConcurrent::mapped(ranges, ComputeRowsTask{m_snapshot, &m_output, rule, boundary});
    m_watcher.setFuture(future);
    *requestId = m_activeId;
    return CommandResult::Success;
}

void SimulationEngine::discardPending()
{
    if (!m_busy)
    {
        return;
    }
    STACKLOG_DEBUG() << "pending step discarded id=" << m_activeId;
    m_activeId = 0;
}

domain::Grid SimulationEngine::takeResult(domain::GridStats* stats)
{
    if (stats != nullptr)
    {
        *stats = m_stats;
        m_stats = domain::GridStats{};
    }
    // Grid owns a heap vector; move it instead of copying the active cells.
    return std::move(m_output);
}

void SimulationEngine::onFinished()
{
    m_busy = false;
    const quint64 finishedId = m_activeId;
    const int expectedTasks = m_expectedTasks;
    m_activeId = 0;
    m_expectedTasks = 0;
    if (finishedId == 0)
    {
        return;
    }
    if (expectedTasks > 0 && m_watcher.future().resultCount() < expectedTasks)
    {
        STACKLOG_ERROR() << "step computation incomplete results="
                         << m_watcher.future().resultCount() << " expected=" << expectedTasks;
        m_stats = domain::GridStats{};
        emit stepFailed(CommandResult::ComputationFailed);
        return;
    }

    domain::GridStats total;
    for (const domain::GridStats& part : m_watcher.future().results())
    {
        total.population += part.population;
        total.changed = total.changed || part.changed;
    }
    m_stats = total;
    emit stepReady(finishedId);
}

} // namespace lifegame::model
