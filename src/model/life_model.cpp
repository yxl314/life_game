#include "model/life_model.h"

#include <stacklog/stacklog.hpp>

#include <QMetaObject>
#include <QMetaType>
#include <QtConcurrent>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <new>
#include <utility>

namespace lifegame::model
{

namespace
{

std::shared_ptr<const domain::Grid> makeSnapshot(const domain::Grid& board)
{
    domain::Grid* copy = new (std::nothrow) domain::Grid(board);
    if (copy == nullptr)
    {
        return {};
    }
    return std::shared_ptr<const domain::Grid>(copy);
}

/**
 * @brief Stamp every alive cell of @p pattern into @p board with an offset.
 *
 * Used both when the user places a pattern at the board centre and when a
 * loaded document is turned into a board. Duplicating the double loop means a
 * future change (for example honouring a selection mask) has to be made twice.
 *
 * @param board Target board; cells outside its bounds are ignored.
 * @param pattern Source pattern.
 * @param topRow Row offset applied to the pattern.
 * @param topCol Column offset applied to the pattern.
 */
void stampPattern(domain::Grid& board, const domain::Pattern& pattern,
                  int topRow, int topCol)
{
    for (int row = 0; row < pattern.height(); ++row)
    {
        for (int col = 0; col < pattern.width(); ++col)
        {
            if (pattern.alive(row, col))
            {
                board.setCell(topRow + row, topCol + col, true);
            }
        }
    }
}

} // namespace

LifeModel::LifeModel(QObject* parent)
    : QObject(parent),
      m_board(domain::kDefaultBoardWidth, domain::kDefaultBoardHeight),
      m_history(100)
{
    qRegisterMetaType<shared::LifeStats>("lifegame::shared::LifeStats");
    qRegisterMetaType<StallReason>("lifegame::model::StallReason");
    qRegisterMetaType<shared::JumpInfo>("lifegame::shared::JumpInfo");
    qRegisterMetaType<CommandResult>("lifegame::model::CommandResult");

    connect(&m_engine, &SimulationEngine::stepReady, this, &LifeModel::onStepReady);
    connect(&m_engine, &SimulationEngine::stepFailed, this, &LifeModel::onStepFailed);
    connect(&m_jumpWatcher, &QFutureWatcher<JumpOutcome>::finished, this, &LifeModel::onJumpFinished);

    resetHistory();
}

LifeModel::~LifeModel()
{
    if (m_jumpCancel)
    {
        *m_jumpCancel = true;
    }
    disconnect(&m_jumpWatcher, nullptr, this, nullptr);
    if (m_jumpWatcher.isRunning())
    {
        m_jumpWatcher.waitForFinished();
    }
}

const domain::Grid& LifeModel::board() const
{
    return m_board;
}

int LifeModel::width() const
{
    return m_board.width();
}

int LifeModel::height() const
{
    return m_board.height();
}

shared::LifeStats LifeModel::stats() const
{
    return computeStats();
}

const domain::Rule& LifeModel::rule() const
{
    return m_rule;
}

domain::Boundary LifeModel::boundary() const
{
    return m_boundary;
}

bool LifeModel::isRunning() const
{
    return m_running;
}

bool LifeModel::isJumping() const
{
    return m_jumping;
}

int LifeModel::oldestGeneration() const
{
    return m_history.empty() ? m_generation : m_history.oldestGeneration();
}

int LifeModel::snapshotInterval() const
{
    return m_history.interval();
}

std::string LifeModel::toRle(std::string_view title) const
{
    return domain::encodeRle(m_board, m_rule, title);
}

bool LifeModel::canMutate() const
{
    return !m_jumping;
}

void LifeModel::pauseForEdit()
{
    m_engine.discardPending();
    m_pendingStepId = 0;

    if (m_running)
    {
        m_running = false;
        emit runningChanged(false);
    }
}

CommandResult LifeModel::setRunning(bool running)
{
    if (!canMutate())
    {
        STACKLOG_DEBUG() << "setRunning rejected reason=busy";
        return CommandResult::Busy;
    }
    if (!running)
    {
        m_engine.discardPending();
        m_pendingStepId = 0;
    }
    if (m_running == running)
    {
        return CommandResult::NoChange;
    }
    if (running && m_population == 0)
    {
        STACKLOG_INFO() << "simulation start rejected reason=empty_board";
        emit simulationStalled(StallReason::EmptyBoard);
        return CommandResult::EmptyBoard;
    }

    m_running = running;
    STACKLOG_INFO() << (running ? "simulation started" : "simulation stopped")
                    << " generation " << m_generation << " population" << m_population;
    emit runningChanged(running);
    return CommandResult::Success;
}

CommandResult LifeModel::toggleRunning()
{
    return setRunning(!m_running);
}

CommandResult LifeModel::setCell(int row, int col, bool alive)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (!m_board.inBounds(row, col))
    {
        return CommandResult::OutOfBounds;
    }
    if (m_board.cell(row, col) == alive)
    {
        return CommandResult::NoChange;
    }

    pauseForEdit();
    m_board.setCell(row, col, alive);
    m_population += alive ? 1 : -1;
    invalidateHistoryFrom(m_generation);
    emit boardChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::toggleCell(int row, int col)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (!m_board.inBounds(row, col))
    {
        return CommandResult::OutOfBounds;
    }

    pauseForEdit();
    const std::optional<bool> state = m_board.toggleCell(row, col);
    if (!state.has_value())
    {
        return CommandResult::OutOfBounds;
    }
    m_population += *state ? 1 : -1;
    invalidateHistoryFrom(m_generation);
    emit boardChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::setCells(const std::vector<std::pair<int, int>>& cells, bool alive)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (cells.empty())
    {
        return CommandResult::NoChange;
    }

    bool wouldChange = false;
    bool invalid = false;
    for (const auto& cell : cells)
    {
        if (!m_board.inBounds(cell.first, cell.second))
        {
            invalid = true;
        }
        else if (m_board.cell(cell.first, cell.second) != alive)
        {
            wouldChange = true;
        }
    }
    if (!wouldChange)
    {
        return invalid ? CommandResult::OutOfBounds : CommandResult::NoChange;
    }

    pauseForEdit();
    for (const auto& cell : cells)
    {
        m_board.setCell(cell.first, cell.second, alive);
    }
    m_population = m_board.population();
    invalidateHistoryFrom(m_generation);
    emit boardChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::placePattern(const domain::Pattern& pattern, int centerRow, int centerCol)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (pattern.width() <= 0 || pattern.height() <= 0 || pattern.population() == 0 ||
        !pattern.isConsistent())
    {
        STACKLOG_WARN() << "pattern rejected reason=invalid_pattern width=" << pattern.width()
                        << " height=" << pattern.height() << " cells=" << pattern.cells().size();
        return CommandResult::InvalidPattern;
    }

    const int topRow = centerRow - pattern.height() / 2;
    const int topCol = centerCol - pattern.width() / 2;

    bool wouldChange = false;
    bool clipped = false;
    for (int row = 0; row < pattern.height(); ++row)
    {
        for (int col = 0; col < pattern.width(); ++col)
        {
            if (!pattern.alive(row, col))
            {
                continue;
            }
            const int targetRow = topRow + row;
            const int targetCol = topCol + col;
            if (!m_board.inBounds(targetRow, targetCol))
            {
                clipped = true;
            }
            else if (!m_board.cell(targetRow, targetCol))
            {
                wouldChange = true;
            }
        }
    }
    if (!wouldChange)
    {
        return CommandResult::NoChange;
    }

    pauseForEdit();
    stampPattern(m_board, pattern, topRow, topCol);
    m_population = m_board.population();
    invalidateHistoryFrom(m_generation);
    STACKLOG_DEBUG() << "pattern placed center " << centerRow << ' ' << centerCol << " size "
                     << pattern.width() << 'x' << pattern.height() << " clipped " << clipped;
    emit boardChanged();
    emit statusChanged(stats());
    return clipped ? CommandResult::Clipped : CommandResult::Success;
}

CommandResult LifeModel::clear()
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }

    pauseForEdit();
    m_board.clear();
    m_population = 0;
    m_generation = 0;
    resetHistory();
    STACKLOG_INFO() << "board cleared";
    emit boardChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::randomize(int densityPercent)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }

    const int density =
        std::clamp(densityPercent, shared::limits::kDensityMin, shared::limits::kDensityMax);
    const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto seed = static_cast<std::uint32_t>(static_cast<std::uint64_t>(ticks) ^
                                                 reinterpret_cast<std::uintptr_t>(this));
    pauseForEdit();
    m_board.randomize(density, seed);
    m_population = m_board.population();
    m_generation = 0;
    resetHistory();
    STACKLOG_INFO() << "board randomized density " << density << " seed " << seed;
    emit boardChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::resizeBoard(int width, int height)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }

    const int clampedWidth = std::clamp(width, domain::kMinBoardExtent, domain::kMaxBoardWidth);
    const int clampedHeight = std::clamp(height, domain::kMinBoardExtent, domain::kMaxBoardHeight);
    const bool adjusted = clampedWidth != width || clampedHeight != height;

    std::unique_ptr<domain::Grid> board(new (std::nothrow) domain::Grid(clampedWidth, clampedHeight));
    if (!board)
    {
        STACKLOG_ERROR() << "resize failed reason=out_of_memory requested " << width << 'x' << height;
        return CommandResult::OutOfMemory;
    }

    const CommandResult replaced = replaceBoard(std::move(board), 0);
    if (replaced != CommandResult::Success)
    {
        return replaced;
    }
    STACKLOG_INFO() << "board resized requested " << width << 'x' << height << " actual "
                    << clampedWidth << 'x' << clampedHeight << " adjusted " << adjusted;
    return adjusted ? CommandResult::Adjusted : CommandResult::Success;
}

CommandResult LifeModel::setRule(const domain::Rule& rule)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (m_rule == rule)
    {
        return CommandResult::NoChange;
    }

    const std::string oldRule = m_rule.notation();
    m_engine.discardPending();
    m_pendingStepId = 0;
    m_rule = rule;
    resetHistory();
    STACKLOG_INFO() << "rule changed from " << oldRule.c_str() << " to " << m_rule.notation().c_str();
    emit configurationChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::setRuleByNotation(std::string_view notation)
{
    const domain::RuleParseResult parsed = domain::Rule::parse(notation);
    if (parsed.error != domain::RuleParseError::None)
    {
        STACKLOG_WARN() << "invalid rule notation=" << std::string(notation).c_str()
                        << " error=" << domain::ruleParseErrorName(parsed.error);
        return CommandResult::InvalidRule;
    }
    return setRule(parsed.rule);
}

CommandResult LifeModel::setBoundary(domain::Boundary boundary)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (m_boundary == boundary)
    {
        return CommandResult::NoChange;
    }

    m_engine.discardPending();
    m_pendingStepId = 0;
    m_boundary = boundary;
    resetHistory();
    STACKLOG_INFO() << "boundary changed to " << (boundary == domain::Boundary::Wrap ? "wrap" : "dead");
    emit configurationChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::setSnapshotInterval(int generations)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }

    const int interval =
        std::clamp(generations, shared::limits::kSnapshotIntervalMin, shared::limits::kSnapshotIntervalMax);
    if (interval == m_history.interval())
    {
        return CommandResult::NoChange;
    }

    m_engine.discardPending();
    m_pendingStepId = 0;
    STACKLOG_INFO() << "snapshot interval changed to " << interval;
    m_history.setInterval(interval);
    resetHistory();
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::setGeneration(int generation)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }

    const int value = std::max(0, generation);
    if (value == m_generation)
    {
        return CommandResult::NoChange;
    }

    m_engine.discardPending();
    m_pendingStepId = 0;
    m_generation = value;
    resetHistory();
    STACKLOG_INFO() << "generation set to " << value;
    emit statusChanged(stats());
    return CommandResult::Success;
}

CommandResult LifeModel::loadDocument(const domain::DecodedDocument& document)
{
    if (!canMutate())
    {
        STACKLOG_DEBUG() << "document rejected reason=busy";
        return CommandResult::Busy;
    }

    const domain::Pattern& pattern = document.pattern;
    if (!pattern.isConsistent() || pattern.width() <= 0 || pattern.height() <= 0)
    {
        STACKLOG_WARN() << "document rejected reason=invalid_size width=" << pattern.width()
                        << " height=" << pattern.height();
        return CommandResult::InvalidPattern;
    }
    if (pattern.width() > domain::kMaxBoardWidth || pattern.height() > domain::kMaxBoardHeight)
    {
        STACKLOG_WARN() << "document rejected reason=too_large width=" << pattern.width()
                        << " height=" << pattern.height();
        return CommandResult::TooLarge;
    }

    std::optional<domain::Rule> rule;
    if (!document.rule.empty())
    {
        const domain::RuleParseResult parsed = domain::Rule::parse(document.rule);
        if (parsed.error != domain::RuleParseError::None)
        {
            STACKLOG_WARN() << "document rejected reason=invalid_rule error="
                            << domain::ruleParseErrorName(parsed.error);
            return CommandResult::InvalidRule;
        }
        rule = parsed.rule;
    }

    const int width = std::max(domain::kMinBoardExtent, pattern.width());
    const int height = std::max(domain::kMinBoardExtent, pattern.height());
    std::unique_ptr<domain::Grid> board(new (std::nothrow) domain::Grid(width, height));
    if (!board)
    {
        STACKLOG_ERROR() << "document load failed reason=out_of_memory";
        return CommandResult::OutOfMemory;
    }
    stampPattern(*board, pattern, 0, 0);

    const CommandResult replaced = replaceBoard(std::move(board), 0);
    if (replaced != CommandResult::Success)
    {
        return replaced;
    }

    if (rule.has_value() && *rule != m_rule)
    {
        m_rule = *rule;
        emit configurationChanged();
    }
    STACKLOG_INFO() << "document loaded board " << width << 'x' << height << " population " << m_population
                    << " rule " << m_rule.notation().c_str();
    return CommandResult::Success;
}

CommandResult LifeModel::step()
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }

    m_engine.discardPending();
    m_pendingStepId = 0;

    domain::Grid next;
    domain::GridStats stats;
    domain::Grid::computeNext(m_board, &next, m_rule, m_boundary, &stats);
    applyStep(std::move(next), stats);
    STACKLOG_DEBUG() << "manual step generation " << m_generation;
    return CommandResult::Success;
}

CommandResult LifeModel::requestStep()
{
    if (m_jumping)
    {
        return CommandResult::Busy;
    }
    if (!m_running)
    {
        return CommandResult::NotRunning;
    }
    if (m_engine.isBusy() || m_pendingStepId != 0)
    {
        return CommandResult::Busy;
    }

    quint64 requestId = 0;
    const CommandResult result = m_engine.request(m_board, m_rule, m_boundary, &requestId);
    if (result == CommandResult::OutOfMemory)
    {
        STACKLOG_ERROR() << "requestStep failed reason=out_of_memory";
    }
    if (result != CommandResult::Success)
    {
        return result;
    }
    m_pendingStepId = requestId;
    return CommandResult::Success;
}

void LifeModel::onStepReady(quint64 requestId)
{
    if (requestId == 0 || requestId != m_pendingStepId)
    {
        STACKLOG_DEBUG() << "stale step discarded id=" << requestId << " pending=" << m_pendingStepId;
        return;
    }
    m_pendingStepId = 0;

    if (!m_running)
    {
        return;
    }

    domain::GridStats stats;
    domain::Grid next = m_engine.takeResult(&stats);
    applyStep(std::move(next), stats);
}

void LifeModel::onStepFailed(CommandResult reason)
{
    m_pendingStepId = 0;
    STACKLOG_ERROR() << "step failed reason=" << commandResultName(reason);
    if (m_running)
    {
        m_running = false;
        emit runningChanged(false);
    }
    emit stepFailed(reason);
}

void LifeModel::applyStep(domain::Grid&& next, const domain::GridStats& gridStats)
{
    const bool unchanged = !gridStats.changed;
    m_board = std::move(next);
    ++m_generation;
    m_population = gridStats.population;
    recordSnapshot();

    emit boardChanged();
    emit statusChanged(stats());

    if (unchanged && m_running)
    {
        const StallReason reason = m_population == 0 ? StallReason::Extinct : StallReason::StillLife;
        STACKLOG_INFO() << "simulation stalled reason=" << stallReasonName(reason)
                        << " generation " << m_generation << " population " << m_population;
        (void)setRunning(false);
        emit simulationStalled(reason);
    }
}

CommandResult LifeModel::startJump(int targetGeneration)
{
    if (!canMutate())
    {
        STACKLOG_DEBUG() << "jump rejected reason=busy";
        return CommandResult::Busy;
    }
    if (targetGeneration < 0 || targetGeneration == m_generation)
    {
        STACKLOG_WARN() << "jump rejected reason=invalid_target target=" << targetGeneration
                        << " current=" << m_generation;
        return CommandResult::InvalidTarget;
    }

    const bool forward = targetGeneration > m_generation;
    int startGeneration = 0;
    std::shared_ptr<const domain::Grid> startBoard;

    if (forward)
    {
        startGeneration = m_generation;
        startBoard = makeSnapshot(m_board);
        if (!startBoard)
        {
            return CommandResult::OutOfMemory;
        }
    }
    else
    {
        const std::optional<domain::History::Snapshot> snapshot =
            m_history.snapshotAtOrBefore(targetGeneration);
        if (!snapshot.has_value())
        {
            STACKLOG_INFO() << "jump rejected reason=no_history target=" << targetGeneration;
            return CommandResult::NoHistory;
        }
        startGeneration = snapshot->generation;
        domain::Grid restored = snapshot->packedBoard->unpack();
        startBoard = std::make_shared<const domain::Grid>(std::move(restored));
    }

    const int total = targetGeneration - startGeneration;
    if (total < 0)
    {
        return CommandResult::InvalidTarget;
    }

    pauseForEdit();
    m_engine.discardPending();
    m_pendingStepId = 0;

    const int epoch = m_history.epochGeneration();
    const int interval = m_history.interval();
    const domain::Rule rule = m_rule;
    const domain::Boundary boundary = m_boundary;
    auto cancel = std::shared_ptr<std::atomic_bool>(new (std::nothrow) std::atomic_bool(false));
    if (!cancel)
    {
        STACKLOG_ERROR() << "jump failed reason=out_of_memory";
        return CommandResult::OutOfMemory;
    }
    m_jumpCancel = cancel;
    m_jumping = true;
    STACKLOG_INFO() << "jump started from " << startGeneration << " to " << targetGeneration
                    << " replay " << total << " forward " << forward;

    JumpRequest request;
    request.startBoard = std::move(startBoard);
    request.startGeneration = startGeneration;
    request.targetGeneration = targetGeneration;
    request.totalGenerations = total;
    request.forward = forward;
    request.rule = rule;
    request.boundary = boundary;
    request.epoch = epoch;
    request.interval = interval;
    request.cancel = std::move(cancel);
    request.reportProgress = [this](int done) {
        QMetaObject::invokeMethod(this,
                                  "reportJumpProgress",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, done));
    };

    const QFuture<JumpOutcome> future = QtConcurrent::run(
        [request = std::move(request)]() noexcept { return runJump(request); });
    m_jumpWatcher.setFuture(future);
    emit jumpStarted(shared::JumpInfo{m_generation, targetGeneration, total});
    return CommandResult::Success;
}

void LifeModel::cancelJump()
{
    if (m_jumpCancel)
    {
        *m_jumpCancel = true;
    }
}

void LifeModel::reportJumpProgress(int done)
{
    STACKLOG_DEBUG() << "jump progress " << done;
    emit jumpProgress(done);
}

void LifeModel::onJumpFinished()
{
    m_jumping = false;
    m_jumpCancel.reset();

    const JumpOutcome outcome = m_jumpWatcher.result();
    if (outcome.cancelled)
    {
        STACKLOG_INFO() << "jump cancelled generation " << m_generation;
        emit jumpCancelled();
        return;
    }
    if (outcome.error != CommandResult::Success)
    {
        STACKLOG_WARN() << "jump failed reason=" << commandResultName(outcome.error);
        emit jumpFailed(outcome.error);
        return;
    }
    if (!outcome.board)
    {
        emit jumpFailed(CommandResult::OutOfMemory);
        return;
    }

    const int previousGeneration = m_generation;
    m_board = *outcome.board;
    m_generation = outcome.generation;
    m_population = m_board.population();

    if (m_generation < previousGeneration)
    {
        m_history.invalidateFrom(m_generation + 1);
    }
    else
    {
        for (const JumpSnapshot& snapshot : outcome.snapshots)
        {
            if (!m_history.recordPacked(snapshot.generation, snapshot.packedBoard))
            {
                STACKLOG_WARN() << "jump snapshot allocation failed generation "
                                << snapshot.generation;
            }
        }
    }

    if (m_history.empty())
    {
        resetHistory();
    }

    STACKLOG_INFO() << "jump finished generation " << m_generation << " population " << m_population;
    emit boardChanged();
    emit statusChanged(stats());
    emit jumpFinished(m_generation);
}

void LifeModel::resetHistory()
{
    if (!m_history.reset(m_generation, makeSnapshot(m_board)))
    {
        STACKLOG_WARN() << "history anchor allocation failed generation " << m_generation;
    }
}

void LifeModel::recordSnapshot()
{
    if (!m_history.shouldRecord(m_generation))
    {
        return;
    }
    std::shared_ptr<const domain::Grid> snapshot = makeSnapshot(m_board);
    if (snapshot && !m_history.record(m_generation, snapshot))
    {
        STACKLOG_WARN() << "history snapshot allocation failed generation " << m_generation;
    }
}

void LifeModel::invalidateHistoryFrom(int generation)
{
    m_history.invalidateFrom(generation);
    if (m_history.empty())
    {
        resetHistory();
    }
}

shared::LifeStats LifeModel::computeStats() const
{
    const int area = m_board.width() * m_board.height();
    const double raw = area > 0 ? 100.0 * static_cast<double>(m_population) / static_cast<double>(area) : 0.0;
    return shared::LifeStats{m_generation, m_population, std::round(raw * 10.0) / 10.0};
}

CommandResult LifeModel::replaceBoard(std::unique_ptr<domain::Grid> board, int generation)
{
    if (!canMutate())
    {
        return CommandResult::Busy;
    }
    if (!board)
    {
        return CommandResult::OutOfMemory;
    }

    const bool sizeChanged =
        board->width() != m_board.width() || board->height() != m_board.height();

    pauseForEdit();
    m_engine.discardPending();
    m_pendingStepId = 0;

    m_board = std::move(*board);
    m_generation = generation;
    m_population = m_board.population();
    resetHistory();

    if (sizeChanged)
    {
        emit boardResized(m_board.width(), m_board.height());
    }
    emit boardChanged();
    emit statusChanged(stats());
    return CommandResult::Success;
}

} // namespace lifegame::model
