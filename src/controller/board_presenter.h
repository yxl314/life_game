#pragma once

/**
 * @file board_presenter.h
 * @brief 模型到视图的呈现器与帧调度。
 */

#include <QMetaObject>
#include <QObject>

#include "shared/app_settings.h"

class QTimer;
class QScreen;
class QWindow;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class BoardView;
class ControlPanel;
class FrameTimingSource;
}

namespace lifegame::controller
{

/**
 * @brief 把模型状态推给被动视图，并负责帧调度。
 *
 * BoardController 只处理输入与命令；本类承担 Model→View 方向，以及“是否
 * 合并重绘”的判断：只有运行中的大棋盘才会按屏幕刷新率节流，其余情况
 * 立即重绘。
 */
class BoardPresenter : public QObject
{
    Q_OBJECT

public:
    /// @brief 连接模型信号并跟踪所在窗口的屏幕刷新率。
    ///
    /// 构造结束前会同步推入首帧。
    /// @param model 被观察的模型。
    /// @param view 被动棋盘视图。
    /// @param panel 被动侧边面板。
    /// @param timing 绘制耗时度量来源。
    /// @param parent QObject 父对象。
    BoardPresenter(model::LifeModel* model,
                   view::BoardView* view,
                   view::ControlPanel* panel,
                   view::FrameTimingSource* timing,
                   QObject* parent = nullptr);

    /// @brief 立即与模型状态同步，绕过帧定时器。
    void sync();

    /// @brief 记录用于合并判断的模拟速率。
    ///
    /// 判定无需合并时，挂起的一帧会立即刷出。
    /// @param generationsPerSecond 当前每秒代数。
    void setGenerationRate(int generationsPerSecond);

private slots:
    /// @brief 立即推入一帧，或推迟到下一次屏幕刷新。
    void requestUpdate();
    /// @brief 取消挂起的帧并推入当前模型状态。
    void updateNow();

private:
    /// @brief 跟踪视图窗口的屏幕切换。
    void watchWindowScreen();
    /// @brief 读取当前屏幕刷新率并重设定时器间隔。
    void refreshScreenRate();
    /// @brief 判断重绘是否需要等到下一次屏幕刷新。
    ///
    /// 只对运行中的大棋盘合并重绘：模拟快于屏幕刷新，或单次绘制超出帧预算。
    /// @return 需要推迟时返回 true。
    bool needsCoalescing() const;

    model::LifeModel* m_model = nullptr;         ///< 被观察的模型。
    view::BoardView* m_view = nullptr;           ///< 被动棋盘视图。
    view::ControlPanel* m_panel = nullptr;       ///< 被动侧边面板。
    view::FrameTimingSource* m_timing = nullptr; ///< 绘制耗时度量来源，可为空。
    QTimer* m_timer = nullptr;                   ///< 单次触发的帧定时器。
    int m_screenRefresh = 60;                    ///< 屏幕刷新率（Hz）。
    int m_generationRate = shared::limits::kDefaultSpeed; ///< 模拟速率（代/秒）。
    QMetaObject::Connection m_screenConnection;       ///< 屏幕刷新率变化订阅。
    QMetaObject::Connection m_windowScreenConnection; ///< 窗口屏幕切换订阅。
};

} // namespace lifegame::controller
