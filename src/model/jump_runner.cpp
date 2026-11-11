#include "model/jump_runner.h"

#include "domain/history.h"

#include <algorithm>
#include <new>
#include <utility>

namespace lifegame::model
{

namespace
{

std::shared_ptr<const domain::Grid> snapshotCopy(const domain::Grid& board)
{
    auto* copy = new (std::nothrow) domain::Grid(board);
    if (copy == nullptr)
    {
        return {};
    }
    return std::shared_ptr<const domain::Grid>(copy);
}

} // namespace

JumpOutcome runJump(const JumpRequest& request) noexcept
{
    JumpOutcome outcome;
    outcome.generation = request.targetGeneration;

    auto current = std::unique_ptr<domain::Grid>(new (std::nothrow) domain::Grid(*request.startBoard));
    auto next = std::unique_ptr<domain::Grid>(
        new (std::nothrow) domain::Grid(request.startBoard->width(), request.startBoard->height()));
    if (!current || !next)
    {
        outcome.error = CommandResult::OutOfMemory;
        return outcome;
    }

    for (int generation = request.startGeneration + 1; generation <= request.targetGeneration; ++generation)
    {
        domain::Grid::computeNext(*current, next.get(), request.rule, request.boundary);
        std::swap(current, next);

        if (request.cancel && request.cancel->load())
        {
            outcome.cancelled = true;
            return outcome;
        }

        const int done = generation - request.startGeneration;
        if (request.forward && (generation - request.epoch) % request.interval == 0)
        {
            auto snapshot = std::shared_ptr<const domain::PackedGrid>(
                new (std::nothrow) domain::PackedGrid(*current));
            if (!snapshot || !snapshot->valid())
            {
                outcome.error = CommandResult::OutOfMemory;
                return outcome;
            }
            outcome.snapshots.push_back({generation, std::move(snapshot)});
            if (outcome.snapshots.size() >
                static_cast<std::size_t>(domain::History::kMaxSnapshots))
            {
                outcome.snapshots.erase(outcome.snapshots.begin());
            }
        }

        if (done % 64 == 0 && current->population() == 0)
        {
            break;
        }

        if (done % 64 == 0 && request.reportProgress)
        {
            request.reportProgress(done);
        }
    }

    outcome.board = snapshotCopy(*current);
    if (!outcome.board)
    {
        outcome.error = CommandResult::OutOfMemory;
    }
    return outcome;
}

} // namespace lifegame::model
