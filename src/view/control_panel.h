#pragma once

/**
 * @file control_panel.h
 * @brief 被动侧边面板与用户意图信号。
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

/// @brief 图案下拉框中的一项。
struct PatternOption
{
    QString text; ///< 本地化标签。
    QString id;   ///< 稳定的图案 ID；空字符串表示画笔。
};

/**
 * @brief 被动侧边面板：控制器推入显示状态，面板发出用户意图。
 *
 * 不依赖模型或控制器。
 */
class ControlPanel : public QWidget
{
    Q_OBJECT

public:
    /// @brief 构造面板及其全部控件。
    /// @param parent 父控件。
    explicit ControlPanel(QWidget* parent = nullptr);

    /// @brief 设置规则下拉框使用的展示目录。
    /// @param catalog 展示目录；可以为空。
    void setRuleCatalog(const RuleCatalog* catalog);

    /// @brief 一次性应用一整帧面板状态。
    /// @param state 统计、尺寸、规则、边界、最早代数与运行状态。
    void applyState(const PanelState& state);

    /// @brief 更新统计数字。
    /// @param stats 要显示的统计；负值按 0 显示。
    void setStats(const shared::LifeStats& stats);

    /// @brief 更新运行状态与开始/暂停按钮。
    /// @param running 运行状态。
    void setRunning(bool running);

    /// @brief 更新尺寸输入框。
    /// @param width 棋盘宽度。
    /// @param height 棋盘高度。
    void setBoardSize(int width, int height);

    /// @brief 更新规则记法并同步下拉框选择。
    /// @param notation 规范 B/S 记法。
    void setRuleNotation(const QString& notation);

    /// @brief 更新边界选择。
    /// @param boundary 要显示的边界策略。
    void setBoundary(domain::Boundary boundary);

    /// @brief 更新可回退的最早代数。
    /// @param generation 最早可回退代数；负数按 0 处理。
    void setOldestGeneration(int generation);

    /// @brief 填充图案下拉框。
    /// @param options 图案选项。
    void setPatternOptions(const QVector<PatternOption>& options);

    /// @brief 更新图案选择器下方的说明文字。
    /// @param text 说明文字。
    void setPatternDescription(const QString& text);

    /// @brief 设置速度并同步滑块与数值标签。
    /// @param speed 每秒演化代数；0 表示暂停，超出范围时会被夹取。
    void setSpeed(int speed);

    /// @brief 返回当前速度。
    /// @return 每秒演化代数。
    int speed() const;

    /// @brief 返回随机填充密度。
    /// @return 存活百分比。
    int randomDensity() const;

    /// @brief 设置随机填充密度。
    /// @param density 存活百分比，超出范围时会被夹取。
    void setRandomDensity(int density);

    /// @brief 查询网格线是否显示。
    /// @return 勾选时返回 true。
    bool isGridVisible() const;

    /// @brief 设置网格线勾选状态。
    /// @param visible 是否显示网格线。
    void setGridVisible(bool visible);

    /// @brief 设置跳转粒度。
    /// @param step 回退/快进的最小代数；小于 1 时按 1 处理。
    void setJumpStep(int step);

    /// @brief 返回当前跳转代数。
    /// @return 跳转输入框中的代数。
    int jumpGenerations() const;

    /// @brief 设置棋盘尺寸输入框的步进。
    /// @param step 步进格数；小于 1 时按 1 处理。
    void setSizeStep(int step);

signals:
    /// @brief 开始/暂停按钮被点击。
    void playPauseRequested();
    /// @brief 请求单步演化。
    void stepRequested();
    /// @brief 请求清空棋盘。
    void clearRequested();
    /// @brief 请求按指定密度随机填充。
    /// @param density 存活百分比。
    void randomizeRequested(int density);
    /// @brief 密度输入框的值变化。
    /// @param density 新的存活百分比。
    void randomDensityChanged(int density);
    /// @brief 跳转粒度输入框的值变化。
    /// @param generations 新的跳转代数。
    void jumpStepChanged(int generations);
    /// @brief 速度滑块变化。
    /// @param speed 新的每秒演化代数。
    void speedChanged(int speed);
    /// @brief 回退按钮被点击。
    /// @param generations 要回退的代数。
    void rewindRequested(int generations);
    /// @brief 快进按钮被点击。
    /// @param generations 要快进的代数。
    void fastForwardRequested(int generations);
    /// @brief 图案选择变化。
    /// @param patternId 图案 ID；空字符串表示画笔。
    void patternChanged(const QString& patternId);
    /// @brief 规则下拉框选择变化。
    /// @param index 规则目录中的下标。
    void ruleChanged(int index);
    /// @brief 边界选择变化。
    /// @param boundary 新的边界策略。
    void boundaryChanged(domain::Boundary boundary);
    /// @brief 请求按新尺寸重建棋盘。
    /// @param width 新宽度。
    /// @param height 新高度。
    void resizeRequested(int width, int height);
    /// @brief 网格线勾选状态变化。
    /// @param visible 是否显示网格线。
    void gridVisibilityChanged(bool visible);

private:
    /// @brief 创建并布局所有控件，连接用户输入信号。
    void setupUi();
    /// @brief 按当前规则记法重建规则下拉框。
    void syncRuleSelection();
    /// @brief 按当前边界策略同步边界下拉框。
    void syncBoundarySelection();
    /// @brief 根据当前代与最早代数刷新回退/快进按钮和提示。
    void updateJumpState();
    /// @brief 刷新速度文本与提示。
    void updateSpeedLabel();

    const RuleCatalog* m_ruleCatalog = nullptr; ///< 规则展示目录；可为空。
    QString m_ruleNotation = QStringLiteral("B3/S23"); ///< 当前规则记法。
    domain::Boundary m_boundary = domain::Boundary::Dead; ///< 当前边界策略。

    QLabel* m_generationValue = nullptr; ///< 代数数值标签。
    QLabel* m_populationValue = nullptr; ///< 存活数数值标签。
    QLabel* m_densityValue = nullptr; ///< 密度数值标签。

    QPushButton* m_playButton = nullptr; ///< 开始/暂停按钮。
    QPushButton* m_stepButton = nullptr; ///< 单步按钮。
    QPushButton* m_clearButton = nullptr; ///< 清空按钮。
    QPushButton* m_randomButton = nullptr; ///< 随机填充按钮。
    QSpinBox* m_densitySpin = nullptr; ///< 随机填充密度输入框。

    QSlider* m_speedSlider = nullptr; ///< 速度滑块。
    QLabel* m_speedValue = nullptr; ///< 速度数值标签。
    int m_speed = shared::limits::kDefaultSpeed; ///< 当前每秒演化代数。

    QSpinBox* m_jumpSpin = nullptr; ///< 跳转代数输入框。
    int m_jumpStep = 100; ///< 跳转粒度。
    QPushButton* m_rewindButton = nullptr; ///< 回退按钮。
    QPushButton* m_forwardButton = nullptr; ///< 快进按钮。
    QPushButton* m_applySizeButton = nullptr; ///< 应用尺寸按钮。

    QComboBox* m_patternCombo = nullptr; ///< 图案下拉框。
    QLabel* m_patternText = nullptr; ///< 图案说明标签。

    QComboBox* m_ruleCombo = nullptr; ///< 规则下拉框。
    QComboBox* m_boundaryCombo = nullptr; ///< 边界下拉框。

    QSpinBox* m_widthSpin = nullptr; ///< 宽度输入框。
    QSpinBox* m_heightSpin = nullptr; ///< 高度输入框。
    QCheckBox* m_gridCheck = nullptr; ///< 网格线勾选框。

    int m_currentGeneration = 0; ///< 已显示的代数。
    int m_population = 0; ///< 已显示的存活数。
    double m_density = -1.0; ///< 已显示的密度；-1 表示尚未设置。
    int m_oldestGeneration = 0; ///< 可回退的最早代数。
    int m_panelWidth = 0; ///< 面板当前显示的宽度。
    int m_panelHeight = 0; ///< 面板当前显示的高度。
    bool m_running = false; ///< 已显示的运行状态。
    bool m_syncing = false; ///< 是否正在程序化同步下拉框。
};

} // namespace lifegame::view
