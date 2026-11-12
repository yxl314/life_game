#pragma once

/**
 * @file life_model.h
 * @brief 可观察的生命游戏应用模型。
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
 * @brief 生命游戏的可观察应用模型。
 *
 * 分层 MVC 约定：
 *  - 模型不依赖视图、服务或控制器。
 *  - 状态变化通过信号广播；控制器订阅信号，再把状态推给被动视图。
 *  - 这里不做文件 I/O、弹窗、QSettings，也不存放本地化字符串。
 *
 * 所有公开方法都在 UI 线程调用。一步演化的异步管线交给
 * SimulationEngine；异步跳转只读取棋盘快照，并通过队列调用回报进度。
 */
class LifeModel : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 以默认尺寸的棋盘构造模型。
     * @param parent QObject 父对象。
     */
    explicit LifeModel(QObject* parent = nullptr);
    /// @brief 取消进行中的跳转并等待后台任务结束。
    ~LifeModel() override;

    /// @brief 返回当前棋盘。
    /// @return 当前棋盘的只读引用。
    const domain::Grid& board() const;

    /// @brief 返回当前棋盘宽度。
    /// @return 列数。
    int width() const;

    /// @brief 返回当前棋盘高度。
    /// @return 行数。
    int height() const;

    /// @brief 返回当前的代数、存活数与密度。
    /// @return 统计快照。
    shared::LifeStats stats() const;

    /// @brief 返回当前规则。
    /// @return 规则的只读引用。
    const domain::Rule& rule() const;

    /// @brief 返回当前边界策略。
    /// @return 边界策略。
    domain::Boundary boundary() const;

    /// @brief 查询模拟时钟是否在运行。
    /// @return 运行中返回 true。
    bool isRunning() const;

    /// @brief 查询是否有后台跳转在进行。
    /// @return 跳转中返回 true。
    bool isJumping() const;

    /// @brief 返回仍可回退到的最早代数。
    /// @return 最早可达代数；历史为空时返回当前代数。
    int oldestGeneration() const;

    /// @brief 返回快照间隔。
    /// @return 每隔多少代记录一张回退快照。
    int snapshotInterval() const;

    /// @brief 把当前棋盘和规则编码成 RLE 文本。
    /// @param title 可选的 RLE 标题。
    /// @return RLE 文本；不进行文件 I/O。
    std::string toRle(std::string_view title = {}) const;

    // ---- commands (controllers only) ----
    /// @brief 设置一个细胞的状态。
    /// @param row 行下标。
    /// @param col 列下标。
    /// @param alive 目标状态。
    /// @return Success、NoChange、OutOfBounds 或 Busy。
    CommandResult setCell(int row, int col, bool alive);

    /// @brief 切换一个细胞的状态。
    /// @param row 行下标。
    /// @param col 列下标。
    /// @return Success、OutOfBounds 或 Busy。
    CommandResult toggleCell(int row, int col);

    /// @brief 批量设置一组坐标的细胞状态。
    /// @param cells 坐标列表，每个元素为（行，列）。
    /// @param alive 目标状态。
    /// @param cells 中越界的坐标会被忽略；没有可修改的界内细胞时，
    ///              含越界坐标返回 OutOfBounds，否则返回 NoChange。
    /// @return Success、NoChange、OutOfBounds 或 Busy。
    CommandResult setCells(const std::vector<std::pair<int, int>>& cells, bool alive);

    /// @brief 以指定中心放置一个图案。
    /// @param pattern 要放置的图案。
    /// @param centerRow 中心行。
    /// @param centerCol 中心列。
    /// @return Success、Clipped、NoChange、Busy 或 InvalidPattern。
    CommandResult placePattern(const domain::Pattern& pattern, int centerRow, int centerCol);

    /// @brief 清空棋盘并回到第 0 代。
    /// @return Success 或 Busy。
    CommandResult clear();

    /// @brief 按密度随机填充棋盘并回到第 0 代。
    /// @param densityPercent 存活百分比，超出范围时会被夹取。
    /// @return Success 或 Busy。
    CommandResult randomize(int densityPercent);

    /// @brief 重建指定尺寸的空棋盘并回到第 0 代。
    /// @param width 请求宽度，超出范围时会被夹取。
    /// @param height 请求高度，超出范围时会被夹取。
    /// @return Success、Adjusted、Busy 或 OutOfMemory。
    CommandResult resizeBoard(int width, int height);

    /// @brief 替换规则并重置回退历史。
    /// @param rule 新规则。
    /// @return Success、NoChange 或 Busy。
    CommandResult setRule(const domain::Rule& rule);

    /// @brief 按 B/S 记法解析并替换规则。
    /// @param notation B/S 记法。
    /// @return Success、NoChange、InvalidRule 或 Busy。
    CommandResult setRuleByNotation(std::string_view notation);

    /// @brief 替换边界策略并重置回退历史。
    /// @param boundary 新边界策略。
    /// @return Success、NoChange 或 Busy。
    CommandResult setBoundary(domain::Boundary boundary);

    /// @brief 设置快照间隔并重建回退历史。
    /// @param generations 快照间隔，超出范围时会被夹取。
    /// @return Success、NoChange 或 Busy。
    CommandResult setSnapshotInterval(int generations);

    /// @brief 把当前代数设为指定值并重置历史。
    /// @param generation 目标代数；负数按 0 处理。
    /// @return Success、NoChange 或 Busy。
    CommandResult setGeneration(int generation);

    /// @brief 用解码后的 RLE 文档替换棋盘。
    /// @param document 解码后的 RLE 文档。
    /// @return Success、Busy、InvalidPattern、TooLarge、InvalidRule 或 OutOfMemory。
    CommandResult loadDocument(const domain::DecodedDocument& document);

    /// @brief 在当前线程同步演化一代。
    /// @return 演化后返回 Success；跳转中返回 Busy。
    CommandResult step();

    /// @brief 请求异步演化一代。
    /// @return Success、Busy、NotRunning 或 OutOfMemory。
    CommandResult requestStep();

    /// @brief 开始或暂停模拟时钟。
    /// @param running 目标运行状态。
    /// @return Success、NoChange、EmptyBoard 或 Busy。
    CommandResult setRunning(bool running);

    /// @brief 反转模拟时钟的运行状态。
    /// @return 与 setRunning() 相同的命令结果。
    CommandResult toggleRunning();

    /// @brief 启动后台回退或快进。
    /// @param targetGeneration 目标代数。
    /// @return Success、NoHistory、InvalidTarget、Busy 或 OutOfMemory。
    CommandResult startJump(int targetGeneration);

    /// @brief 请求协作取消当前跳转。
    void cancelJump();

signals:
    /// @brief 棋盘细胞、存活数或布局变化时发出。
    void boardChanged();

    /// @brief 棋盘尺寸变化时发出。
    /// @param width 新宽度。
    /// @param height 新高度。
    void boardResized(int width, int height);

    /// @brief 代数、存活数或密度变化时发出。
    /// @param stats 新统计。
    void statusChanged(const lifegame::shared::LifeStats& stats);

    /// @brief 运行状态变化时发出。
    /// @param running 新状态。
    void runningChanged(bool running);

    /// @brief 规则或边界变化时发出。
    void configurationChanged();

    /// @brief 模拟因停滞而停止时发出。
    /// @param reason 停滞原因。
    void simulationStalled(lifegame::model::StallReason reason);

    /// @brief 后台计算失败时发出。
    /// @param reason 失败码。
    void stepFailed(lifegame::model::CommandResult reason);

    /// @brief 回退或快进开始时发出。
    /// @param info 跳转参数。
    void jumpStarted(const lifegame::shared::JumpInfo& info);

    /// @brief 跳转过程中周期性发出。
    /// @param done 已重放的代数。
    void jumpProgress(int done);

    /// @brief 跳转完成时发出。
    /// @param generation 最终代数。
    void jumpFinished(int generation);

    /// @brief 跳转被取消时发出。
    void jumpCancelled();

    /// @brief 跳转失败时发出。
    /// @param reason 失败码。
    void jumpFailed(lifegame::model::CommandResult reason);

private slots:
    /// @brief 接收后台一步计算的结果。
    /// @param requestId 请求编号；与挂起编号不符时丢弃该结果。
    void onStepReady(quint64 requestId);
    /// @brief 处理后台一步计算的失败并停止模拟。
    /// @param reason 失败码。
    void onStepFailed(lifegame::model::CommandResult reason);
    /// @brief 收集后台跳转的结果并更新棋盘与历史。
    void onJumpFinished();
    /// @brief 把工作线程的进度转成 jumpProgress 信号。
    /// @param done 已重放的代数。
    void reportJumpProgress(int done);

private:
    /// @brief 丢弃挂起的异步一步；模拟运行中时同时暂停。
    void pauseForEdit();
    /// @brief 以当前代和棋盘重建回退历史。
    void resetHistory();
    /// @brief 达到快照间隔时记录当前代。
    void recordSnapshot();
    /// @brief 丢弃指定代及之后的历史快照。
    /// @param generation 起始代数。
    void invalidateHistoryFrom(int generation);
    /// @brief 计算当前代数、存活数与密度。
    /// @return 统计值。
    shared::LifeStats computeStats() const;
    /// @brief 棋盘变化的集中通知入口。
    void notifyBoardChanged();
    /// @brief 状态变化的集中通知入口。
    void notifyStatusChanged();
    /// @brief 采用一次计算出的棋盘与统计，并检查是否停滞。
    /// @param next 新一代棋盘。
    /// @param gridStats 该代计算得到的统计。
    void applyStep(domain::Grid&& next, const domain::GridStats& gridStats);
    /// @brief 用新棋盘替换当前棋盘并重置历史。
    /// @param board 新棋盘；不能为空。
    /// @param generation 新棋盘对应的代数。
    /// @return Success、Busy 或 OutOfMemory。
    CommandResult replaceBoard(std::unique_ptr<domain::Grid> board, int generation);
    /// @brief 判断当前是否允许修改棋盘。
    /// @return 没有进行中的跳转时为 true。
    bool canMutate() const;

    domain::Grid m_board; ///< 当前棋盘。
    domain::Rule m_rule; ///< 当前规则。
    domain::Boundary m_boundary = domain::Boundary::Dead; ///< 边界策略。
    int m_generation = 0; ///< 当前代数。
    int m_population = 0; ///< 当前存活细胞数。
    bool m_running = false; ///< 模拟时钟是否在运行。

    domain::History m_history; ///< 回退快照历史。
    SimulationEngine m_engine; ///< 异步一步计算引擎。
    quint64 m_pendingStepId = 0; ///< 挂起的一步请求编号；0 表示没有。

    QFutureWatcher<JumpOutcome> m_jumpWatcher; ///< 后台跳转任务的完成监视器。
    std::shared_ptr<std::atomic_bool> m_jumpCancel; ///< 协作取消标志。
    bool m_jumping = false; ///< 是否有后台跳转在进行。
};

} // namespace lifegame::model
