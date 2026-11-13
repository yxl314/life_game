#pragma once

/**
 * @file simulation_engine.h
 * @brief 异步的一代计算引擎。
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
 * @brief 异步计算一代演化。
 *
 * 引擎持有输入快照和输出缓冲区。`request()` 返回单调递增的编号；
 * 计算完成时在 UI 线程发出 `stepReady(id)`。`discardPending()` 让在途结果
 * 失效（工作线程仍会跑完，但不会发出信号）。在途计算回收前 request()
 * 返回 Busy；需要等 onFinished 之后才能发起下一次请求。
 *
 * 一代计算的 QtConcurrent 管线由本类独占；跳转重放也使用 QtConcurrent，
 * 但放在 LifeModel 中，与本类保持分离，使 LifeModel 专注状态与不变量。
 */
class SimulationEngine : public QObject
{
    Q_OBJECT

public:
    /// @brief 构造引擎并连接完成回调。
    /// @param parent QObject 父对象。
    explicit SimulationEngine(QObject* parent = nullptr);
    /// @brief 断开连接并等待在途计算结束。
    ~SimulationEngine() override;

    /// @brief 查询是否有计算在途。
    /// @return 在途时返回 true。
    bool isBusy() const;

    /**
     * @brief 启动一次异步的一代计算。
     * @param board 当前棋盘的只读引用。
     * @param rule 要应用的规则。
     * @param boundary 边界策略。
     * @param requestId 输出请求编号；不能为空。
     * @return Success、Busy、InvalidArgument、OutOfBounds 或 OutOfMemory。
     */
    CommandResult request(const domain::Grid& board,
                          const domain::Rule& rule,
                          domain::Boundary boundary,
                          quint64* requestId);

    /// @brief 让在途请求失效；不会中断工作线程。
    void discardPending();

    /// @brief 取出计算完成的棋盘。
    /// @param stats 可选的输出统计；为空时不返回统计。
    /// @return 完成的棋盘；必须在 stepReady() 之后调用。
    domain::Grid takeResult(domain::GridStats* stats = nullptr);

signals:
    /// @brief 一代计算完成时在 UI 线程发出。
    /// @param requestId request() 返回的编号。
    void stepReady(quint64 requestId);

    /// @brief 计算失败或不完整时发出。
    /// @param reason 失败码。
    void stepFailed(lifegame::model::CommandResult reason);

private slots:
    /// @brief 汇总分块结果并发出完成或失败信号。
    void onFinished();

private:
    QFutureWatcher<domain::GridStats> m_watcher; ///< 分块计算的完成监视器。
    domain::Grid m_output; ///< 输出棋盘。
    domain::GridStats m_stats; ///< 汇总后的统计。
    std::shared_ptr<const domain::Grid> m_snapshot; ///< 当前计算的输入快照。
    quint64 m_activeId = 0; ///< 当前请求编号；0 表示已失效。
    quint64 m_nextId = 1; ///< 下一个可用的请求编号。
    bool m_busy = false; ///< 是否有计算在途。
    int m_expectedTasks = 0; ///< 预期的分块结果数。
};

} // namespace lifegame::model
