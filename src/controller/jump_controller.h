#pragma once

/**
 * @file jump_controller.h
 * @brief 回退/快进控制器与进度界面。
 */

#include <QObject>

#include <memory>

#include "model/command_result.h"
#include "view/interfaces.h"

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class ControlPanel;
class MessageSink;
}

namespace lifegame::controller
{

/**
 * @brief 回退/快进的输入处理与进度展示。
 *
 * 把面板/菜单意图翻译成模型的跳转请求，并把模型的进度信号转给由工厂
 * 创建的进度界面。跳转在模型内部异步完成；本类自己不推进代数。
 */
class JumpController : public QObject
{
    Q_OBJECT

public:
    /// @brief 恢复跳转粒度，并连接面板与模型信号。
    /// @param model 被命令的模型。
    /// @param panel 跳转控件。
    /// @param messages 状态栏输出端口。
    /// @param factory 进度界面工厂，测试中可以为空。
    /// @param parent QObject 父对象。
    JumpController(model::LifeModel* model,
                   view::ControlPanel* panel,
                   view::MessageSink* messages,
                   view::JumpProgressFactory factory,
                   QObject* parent = nullptr);

public slots:
    /// @brief 按面板粒度回退。
    void rewind();
    /// @brief 按面板粒度快进。
    void fastForward();
    /// @brief 按带符号的代数跳转。
    ///
    /// 目标会被钳制到仍保留的最早一代；已有跳转进行中时拒绝，钳制后的
    /// 目标等于当前代时直接返回。
    /// @param delta 正数前进，负数后退。
    void jumpBy(int delta);

    /// @brief 查询跳转粒度。
    /// @return 跳转代数。
    int jumpStep() const;

    /// @brief 修改跳转粒度。
    /// @param step 跳转代数；按设置范围钳制。
    void setJumpStep(int step);

private slots:
    /// @brief 按调用方给定的代数回退。
    /// @param generations 回退的代数。
    void rewindBy(int generations);
    /// @brief 为刚开始的跳转创建并显示进度界面。
    /// @param info 本次跳转的参数。
    void onJumpStarted(const lifegame::shared::JumpInfo& info);
    /// @brief 把已重放的代数推给进度界面。
    /// @param done 已重放的代数。
    void onJumpProgress(int done);
    /// @brief 关闭进度界面并报告到达的代数。
    /// @param generation 跳转到达的代数。
    void onJumpFinished(int generation);
    /// @brief 用户取消跳转后关闭进度界面。
    void onJumpCancelled();
    /// @brief 关闭进度界面并报告失败原因。
    /// @param reason 中止跳转的模型结果。
    void onJumpFailed(lifegame::model::CommandResult reason);

private:
    /// @brief 关闭并释放进度界面（若已打开）。
    void closeProgress();

    model::LifeModel* m_model = nullptr;                ///< 被命令的模型。
    view::ControlPanel* m_panel = nullptr;              ///< 跳转控件。
    view::MessageSink* m_messages = nullptr;            ///< 状态栏输出端口。
    view::JumpProgressFactory m_factory;                ///< 进度界面工厂；为空时不显示进度。
    std::unique_ptr<view::JumpProgressView> m_progress; ///< 当前打开的进度界面。
    int m_jumpStep = 100;                               ///< 跳转代数。
};

} // namespace lifegame::controller
