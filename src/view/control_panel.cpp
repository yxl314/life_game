#include "view/control_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

#include "domain/board_limits.h"
#include "domain/rule.h"
#include "shared/app_settings.h"
#include "view/rule_catalog.h"
#include "view/speed_scale.h"
#include "view/theme.h"

namespace lifegame::view
{

namespace
{

QWidget* makeStat(const QString& iconPath, const QString& caption, QLabel** value, QWidget* parent)
{
    auto* container = new QWidget(parent);
    auto* layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);

    auto* label = new QLabel(QStringLiteral("0"), container);
    label->setObjectName(QStringLiteral("StatValue"));
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    auto* icon = new QLabel(container);
    icon->setPixmap(QIcon(iconPath).pixmap(12, 12));
    icon->setFixedSize(12, 12);

    auto* captionLabel = new QLabel(caption, container);
    captionLabel->setObjectName(QStringLiteral("StatCaption"));

    auto* captionRow = new QHBoxLayout();
    captionRow->setContentsMargins(0, 0, 0, 0);
    captionRow->setSpacing(4);
    captionRow->addWidget(icon);
    captionRow->addWidget(captionLabel);
    captionRow->addStretch(1);

    layout->addWidget(label);
    layout->addLayout(captionRow);
    *value = label;
    return container;
}

} // namespace

ControlPanel::ControlPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("SidePanel"));
    setMinimumWidth(270);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    setupUi();
}

void ControlPanel::setupUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 12, 14, 8);
    root->setSpacing(7);

    auto* header = new QHBoxLayout();
    header->setSpacing(10);
    auto* logo = new QLabel(this);
    logo->setPixmap(theme::appIcon().pixmap(36, 36));
    logo->setFixedSize(36, 36);
    logo->setToolTip(tr("生命游戏 · Conway's Game of Life"));

    auto* titles = new QVBoxLayout();
    titles->setSpacing(0);
    auto* title = new QLabel(tr("生命游戏"), this);
    title->setObjectName(QStringLiteral("Title"));
    auto* subtitle = new QLabel(QStringLiteral("CONWAY'S GAME OF LIFE"), this);
    subtitle->setObjectName(QStringLiteral("Subtitle"));
    titles->addWidget(title);
    titles->addWidget(subtitle);

    header->addWidget(logo);
    header->addLayout(titles, 1);
    root->addLayout(header);

    auto* stats = new QFrame(this);
    stats->setObjectName(QStringLiteral("Card"));
    auto* statsLayout = new QHBoxLayout(stats);
    statsLayout->setContentsMargins(10, 8, 10, 8);
    statsLayout->setSpacing(8);
    statsLayout->addWidget(makeStat(QStringLiteral(":/icons/hash.svg"), tr("代数"), &m_generationValue, stats));
    statsLayout->addWidget(makeStat(QStringLiteral(":/icons/users.svg"), tr("存活"), &m_populationValue, stats));
    statsLayout->addWidget(makeStat(QStringLiteral(":/icons/percent.svg"), tr("密度"), &m_densityValue, stats));
    m_densityValue->setText(QStringLiteral("0.0%"));
    root->addWidget(stats);

    m_playButton = new QPushButton(tr("开始"), this);
    m_playButton->setObjectName(QStringLiteral("PrimaryButton"));
    m_playButton->setMinimumHeight(36);
    m_playButton->setIcon(QIcon(QStringLiteral(":/icons/play.svg")));
    m_playButton->setAccessibleName(tr("开始或暂停模拟"));
    m_playButton->setToolTip(tr("开始或暂停模拟（空格）"));
    connect(m_playButton, &QPushButton::clicked, this, &ControlPanel::playPauseRequested);
    root->addWidget(m_playButton);

    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);
    m_stepButton = new QPushButton(tr("单步"), this);
    m_stepButton->setIcon(QIcon(QStringLiteral(":/icons/step-forward.svg")));
    m_stepButton->setAccessibleName(tr("单步演化一代"));
    m_stepButton->setToolTip(tr("只演化一代（F6）"));
    connect(m_stepButton, &QPushButton::clicked, this, &ControlPanel::stepRequested);

    m_clearButton = new QPushButton(tr("清空"), this);
    m_clearButton->setIcon(QIcon(QStringLiteral(":/icons/trash-2.svg")));
    m_clearButton->setAccessibleName(tr("清空棋盘"));
    m_clearButton->setToolTip(tr("清空棋盘并停止模拟"));
    connect(m_clearButton, &QPushButton::clicked, this, &ControlPanel::clearRequested);

    actionRow->addWidget(m_stepButton);
    actionRow->addWidget(m_clearButton);
    root->addLayout(actionRow);

    auto* randomRow = new QHBoxLayout();
    randomRow->setSpacing(8);
    m_randomButton = new QPushButton(tr("随机填充"), this);
    m_randomButton->setIcon(QIcon(QStringLiteral(":/icons/dices.svg")));
    m_randomButton->setAccessibleName(tr("随机填充棋盘"));
    m_randomButton->setToolTip(tr("按右侧密度随机撒点"));
    m_densitySpin = new QSpinBox(this);
    m_densitySpin->setRange(shared::limits::kDensityMin, shared::limits::kDensityMax);
    m_densitySpin->setValue(shared::AppSettings{}.density);
    m_densitySpin->setSuffix(tr(" %"));
    m_densitySpin->setFixedWidth(72);
    m_densitySpin->setAccessibleName(tr("随机填充密度"));
    m_densitySpin->setToolTip(tr("随机填充的存活概率"));
    connect(m_randomButton, &QPushButton::clicked, this, [this] {
        emit randomizeRequested(m_densitySpin->value());
    });
    connect(m_densitySpin, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &ControlPanel::randomDensityChanged);
    randomRow->addWidget(m_randomButton, 1);
    randomRow->addWidget(m_densitySpin);
    root->addLayout(randomRow);

    auto* speedRow = new QHBoxLayout();
    speedRow->setSpacing(8);
    auto* speedIcon = new QLabel(this);
    speedIcon->setPixmap(QIcon(QStringLiteral(":/icons/gauge.svg")).pixmap(14, 14));
    speedIcon->setFixedSize(14, 14);
    auto* speedTitle = new QLabel(tr("速度"), this);
    speedTitle->setObjectName(QStringLiteral("Muted"));
    m_speedSlider = new QSlider(Qt::Horizontal, this);
    m_speedSlider->setRange(0, kSpeedSliderSteps);
    m_speedSlider->setSingleStep(1);
    m_speedSlider->setPageStep(10);
    m_speedSlider->setValue(sliderPositionForSpeed(m_speed));
    m_speedSlider->setAccessibleName(tr("模拟速度"));
    m_speedSlider->setToolTip(tr("模拟速度：每秒演化代数（最高 %1）").arg(shared::limits::kSpeedMax));
    m_speedValue = new QLabel(this);
    m_speedValue->setObjectName(QStringLiteral("ValueLabel"));
    m_speedValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_speedValue->setMinimumWidth(76);
    connect(m_speedSlider, &QSlider::valueChanged, this, [this](int position) {
        const int speed = speedForSliderPosition(position);
        m_speedValue->setText(tr("%1 代/秒").arg(speed));
        m_speedValue->setToolTip(speed == 0 ? tr("速度为 0：模拟暂停") : tr("每秒演化代数"));
        if (speed != m_speed)
        {
            m_speed = speed;
            emit speedChanged(speed);
        }
    });
    speedRow->addWidget(speedIcon);
    speedRow->addWidget(speedTitle);
    speedRow->addWidget(m_speedSlider, 1);
    speedRow->addWidget(m_speedValue);
    root->addLayout(speedRow);
    setSpeed(m_speed);

    auto* jumpRow = new QHBoxLayout();
    jumpRow->setSpacing(6);
    auto* jumpIcon = new QLabel(this);
    jumpIcon->setPixmap(QIcon(QStringLiteral(":/icons/arrow-left-right.svg")).pixmap(14, 14));
    jumpIcon->setFixedSize(14, 14);
    jumpIcon->setToolTip(tr("按代数回退或快进"));

    m_jumpSpin = new QSpinBox(this);
    m_jumpSpin->setObjectName(QStringLiteral("JumpSpin"));
    m_jumpSpin->setRange(m_jumpStep, 10000);
    m_jumpSpin->setSingleStep(m_jumpStep);
    m_jumpSpin->setValue(m_jumpStep);
    m_jumpSpin->setSuffix(tr(" 代"));
    m_jumpSpin->setFixedWidth(80);
    m_jumpSpin->setKeyboardTracking(false);
    m_jumpSpin->setAccessibleName(tr("跳转代数"));
    m_jumpSpin->setToolTip(tr("一次回退或快进的代数"));
    connect(m_jumpSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int value) {
        updateJumpState();
        emit jumpStepChanged(value);
    });

    m_rewindButton = new QPushButton(QIcon(QStringLiteral(":/icons/rewind.svg")), tr("回退"), this);
    m_rewindButton->setFixedWidth(72);
    m_rewindButton->setAccessibleName(tr("回退指定代数"));
    connect(m_rewindButton, &QPushButton::clicked, this, [this] {
        emit rewindRequested(m_jumpSpin->value());
    });

    m_forwardButton = new QPushButton(QIcon(QStringLiteral(":/icons/fast-forward.svg")), tr("快进"), this);
    m_forwardButton->setFixedWidth(72);
    m_forwardButton->setAccessibleName(tr("快进指定代数"));
    connect(m_forwardButton, &QPushButton::clicked, this, [this] {
        emit fastForwardRequested(m_jumpSpin->value());
    });

    jumpRow->addWidget(jumpIcon);
    jumpRow->addWidget(m_jumpSpin);
    jumpRow->addWidget(m_rewindButton);
    jumpRow->addWidget(m_forwardButton);
    root->addLayout(jumpRow);
    updateJumpState();

    m_gridCheck = new QCheckBox(tr("显示网格线"), this);
    m_gridCheck->setChecked(true);
    m_gridCheck->setAccessibleName(tr("显示网格线"));
    m_gridCheck->setToolTip(tr("显示或隐藏棋盘网格线（Ctrl+G）"));
    connect(m_gridCheck, &QCheckBox::toggled, this, &ControlPanel::gridVisibilityChanged);
    root->addWidget(m_gridCheck);

    auto* patternGroup = new QGroupBox(tr("图案"), this);
    auto* patternLayout = new QVBoxLayout(patternGroup);
    patternLayout->setContentsMargins(10, 8, 10, 8);
    patternLayout->setSpacing(6);
    m_patternCombo = new QComboBox(patternGroup);
    m_patternCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_patternCombo->setMinimumContentsLength(8);
    m_patternCombo->setAccessibleName(tr("图案"));
    m_patternCombo->setToolTip(tr("选择要放置的图案；选择画笔可回到直接绘制"));
    connect(m_patternCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        emit patternChanged(m_patternCombo->currentData().toString());
    });
    patternLayout->addWidget(m_patternCombo);

    m_patternText = new QLabel(patternGroup);
    m_patternText->setObjectName(QStringLiteral("Muted"));
    m_patternText->setWordWrap(true);
    m_patternText->setMinimumHeight(32);
    m_patternText->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    patternLayout->addWidget(m_patternText);
    root->addWidget(patternGroup);

    auto* ruleGroup = new QGroupBox(tr("规则与边界"), this);
    auto* ruleLayout = new QHBoxLayout(ruleGroup);
    ruleLayout->setContentsMargins(10, 8, 10, 8);
    ruleLayout->setSpacing(10);

    auto* ruleColumn = new QVBoxLayout();
    ruleColumn->setSpacing(3);
    auto* ruleLabel = new QLabel(tr("规则"), ruleGroup);
    ruleLabel->setObjectName(QStringLiteral("Muted"));
    m_ruleCombo = new QComboBox(ruleGroup);
    m_ruleCombo->setObjectName(QStringLiteral("RuleCombo"));
    m_ruleCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_ruleCombo->setMinimumContentsLength(4);
    m_ruleCombo->setAccessibleName(tr("规则"));
    m_ruleCombo->setToolTip(tr("选择生命游戏规则"));
    connect(m_ruleCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (!m_syncing)
        {
            emit ruleChanged(index);
        }
    });
    ruleColumn->addWidget(ruleLabel);
    ruleColumn->addWidget(m_ruleCombo);

    auto* boundaryColumn = new QVBoxLayout();
    boundaryColumn->setSpacing(3);
    auto* boundaryLabel = new QLabel(tr("边界"), ruleGroup);
    boundaryLabel->setObjectName(QStringLiteral("Muted"));
    m_boundaryCombo = new QComboBox(ruleGroup);
    m_boundaryCombo->setObjectName(QStringLiteral("BoundaryCombo"));
    m_boundaryCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_boundaryCombo->setMinimumContentsLength(4);
    m_boundaryCombo->setAccessibleName(tr("边界"));
    m_boundaryCombo->setToolTip(tr("死亡边界：越界视为空；环绕边界：上下左右相连"));
    connect(m_boundaryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (!m_syncing)
        {
            emit boundaryChanged(index == 1 ? domain::Boundary::Wrap : domain::Boundary::Dead);
        }
    });
    boundaryColumn->addWidget(boundaryLabel);
    boundaryColumn->addWidget(m_boundaryCombo);

    ruleLayout->addLayout(ruleColumn, 1);
    ruleLayout->addLayout(boundaryColumn, 1);
    root->addWidget(ruleGroup);

    auto* sizeGroup = new QGroupBox(tr("棋盘尺寸"), this);
    auto* sizeLayout = new QHBoxLayout(sizeGroup);
    sizeLayout->setContentsMargins(10, 8, 10, 8);
    sizeLayout->setSpacing(6);

    m_widthSpin = new QSpinBox(sizeGroup);
    m_widthSpin->setObjectName(QStringLiteral("WidthSpin"));
    m_widthSpin->setRange(domain::kMinBoardExtent, domain::kMaxBoardWidth);
    m_widthSpin->setSingleStep(5);
    m_widthSpin->setValue(domain::kDefaultBoardWidth);
    m_widthSpin->setFixedWidth(72);
    m_widthSpin->setKeyboardTracking(false);
    m_widthSpin->setAccessibleName(tr("棋盘宽度"));
    m_widthSpin->setToolTip(tr("列数（20 - 1500）"));

    m_heightSpin = new QSpinBox(sizeGroup);
    m_heightSpin->setObjectName(QStringLiteral("HeightSpin"));
    m_heightSpin->setRange(domain::kMinBoardExtent, domain::kMaxBoardHeight);
    m_heightSpin->setSingleStep(5);
    m_heightSpin->setValue(domain::kDefaultBoardHeight);
    m_heightSpin->setFixedWidth(72);
    m_heightSpin->setKeyboardTracking(false);
    m_heightSpin->setAccessibleName(tr("棋盘高度"));
    m_heightSpin->setToolTip(tr("行数（20 - 1000）"));

    auto* times = new QLabel(QStringLiteral("×"), sizeGroup);
    times->setObjectName(QStringLiteral("Muted"));

    m_applySizeButton = new QPushButton(QIcon(QStringLiteral(":/icons/check.svg")), tr("应用"), sizeGroup);
    m_applySizeButton->setObjectName(QStringLiteral("ApplySizeButton"));
    m_applySizeButton->setFixedWidth(68);
    m_applySizeButton->setAccessibleName(tr("应用棋盘尺寸"));
    m_applySizeButton->setToolTip(tr("按新尺寸重建空棋盘（原棋盘会被清空）"));
    connect(m_applySizeButton, &QPushButton::clicked, this, [this] {
        emit resizeRequested(m_widthSpin->value(), m_heightSpin->value());
    });

    sizeLayout->addWidget(m_widthSpin, 1);
    sizeLayout->addWidget(times);
    sizeLayout->addWidget(m_heightSpin, 1);
    sizeLayout->addWidget(m_applySizeButton);
    root->addWidget(sizeGroup);

    root->addStretch(1);

    m_panelWidth = m_widthSpin->value();
    m_panelHeight = m_heightSpin->value();
    setRunning(false);
    syncBoundarySelection();
}

void ControlPanel::applyState(const PanelState& state)
{
    setStats(state.stats);
    setRunning(state.running);
    setBoardSize(state.width, state.height);
    setRuleNotation(state.ruleNotation);
    setBoundary(state.boundary);
    setOldestGeneration(state.oldestGeneration);
}

void ControlPanel::setRuleCatalog(const RuleCatalog* catalog)
{
    m_ruleCatalog = catalog;
    syncRuleSelection();
}

void ControlPanel::setPatternOptions(const QVector<PatternOption>& options)
{
    const QSignalBlocker blocker(m_patternCombo);
    m_patternCombo->clear();
    for (const PatternOption& option : options)
    {
        m_patternCombo->addItem(option.text, option.id);
    }

    m_patternCombo->setCurrentIndex(0);
}

void ControlPanel::setPatternDescription(const QString& text)
{
    m_patternText->setText(text);
}

void ControlPanel::setSpeed(int speed)
{
    const int value = std::clamp(speed, shared::limits::kSpeedMin, shared::limits::kSpeedMax);
    m_speed = value;
    {
        const QSignalBlocker blocker(m_speedSlider);
        m_speedSlider->setValue(sliderPositionForSpeed(value));
    }
    updateSpeedLabel();
}

int ControlPanel::speed() const
{
    return m_speed;
}

int ControlPanel::randomDensity() const
{
    return m_densitySpin->value();
}

void ControlPanel::setRandomDensity(int density)
{
    const QSignalBlocker blocker(m_densitySpin);
    m_densitySpin->setValue(std::clamp(density, shared::limits::kDensityMin, shared::limits::kDensityMax));
}

bool ControlPanel::isGridVisible() const
{
    return m_gridCheck->isChecked();
}

void ControlPanel::setGridVisible(bool visible)
{
    const QSignalBlocker blocker(m_gridCheck);
    m_gridCheck->setChecked(visible);
}

void ControlPanel::setJumpStep(int step)
{
    m_jumpStep = std::max(1, step);
    m_jumpSpin->setMinimum(m_jumpStep);
    m_jumpSpin->setSingleStep(m_jumpStep);

    const int rounded = ((m_jumpSpin->value() + m_jumpStep / 2) / m_jumpStep) * m_jumpStep;
    const int aligned = std::max(m_jumpStep, std::min(m_jumpSpin->maximum(), rounded));
    {
        const QSignalBlocker blocker(m_jumpSpin);
        m_jumpSpin->setValue(aligned);
    }
    updateJumpState();
}

int ControlPanel::jumpGenerations() const
{
    return m_jumpSpin->value();
}

void ControlPanel::setSizeStep(int step)
{
    const int value = std::max(1, step);
    m_widthSpin->setSingleStep(value);
    m_heightSpin->setSingleStep(value);
}

void ControlPanel::setStats(const shared::LifeStats& stats)
{
    const int generation = std::max(0, stats.generation);
    const int population = std::max(0, stats.population);
    const double density = std::max(0.0, stats.density);
    if (generation == m_currentGeneration && population == m_population &&
        qFuzzyCompare(density + 1.0, m_density + 1.0))
    {
        return;
    }
    m_currentGeneration = generation;
    m_population = population;
    m_density = density;
    m_generationValue->setText(QLocale().toString(generation));
    m_populationValue->setText(QLocale().toString(population));
    m_densityValue->setText(QStringLiteral("%1%").arg(QString::number(density, 'f', 1)));
    updateJumpState();
}

void ControlPanel::setRunning(bool running)
{
    if (m_running == running)
    {
        return;
    }
    m_running = running;
    m_playButton->setText(running ? tr("暂停") : tr("开始"));
    m_playButton->setIcon(QIcon(running ? QStringLiteral(":/icons/pause.svg")
                                        : QStringLiteral(":/icons/play.svg")));
    m_playButton->setToolTip(running ? tr("暂停模拟（空格）") : tr("开始模拟（空格）"));
}

void ControlPanel::setBoardSize(int width, int height)
{
    const int nextWidth = std::clamp(width, m_widthSpin->minimum(), m_widthSpin->maximum());
    const int nextHeight = std::clamp(height, m_heightSpin->minimum(), m_heightSpin->maximum());
    if (nextWidth == m_panelWidth && nextHeight == m_panelHeight)
    {
        return;
    }
    m_panelWidth = nextWidth;
    m_panelHeight = nextHeight;
    const QSignalBlocker widthBlocker(m_widthSpin);
    const QSignalBlocker heightBlocker(m_heightSpin);
    m_widthSpin->setValue(nextWidth);
    m_heightSpin->setValue(nextHeight);
}

void ControlPanel::setRuleNotation(const QString& notation)
{
    if (m_ruleNotation == notation && m_ruleCombo->count() > 0)
    {
        return;
    }
    m_ruleNotation = notation;
    syncRuleSelection();
}

void ControlPanel::setBoundary(domain::Boundary boundary)
{
    const domain::Boundary next = boundary == domain::Boundary::Wrap ? domain::Boundary::Wrap
                                                                    : domain::Boundary::Dead;
    if (m_boundary == next && m_boundaryCombo->count() > 0)
    {
        return;
    }
    m_boundary = next;
    syncBoundarySelection();
}

void ControlPanel::setOldestGeneration(int generation)
{
    const int next = std::max(0, generation);
    if (m_oldestGeneration == next)
    {
        return;
    }
    m_oldestGeneration = next;
    updateJumpState();
}

void ControlPanel::syncRuleSelection()
{
    if (m_ruleCatalog == nullptr)
    {
        return;
    }

    QStringList names = m_ruleCatalog->names();
    int index = -1;
    const auto& presets = m_ruleCatalog->presets();
    for (std::size_t i = 0; i < presets.size(); ++i)
    {
        if (presets[i].notation == m_ruleNotation)
        {
            index = static_cast<int>(i);
            break;
        }
    }
    if (index < 0)
    {
        names.append(tr("自定义 · %1").arg(m_ruleNotation));
        index = static_cast<int>(names.size()) - 1;
    }

    m_syncing = true;
    {
        const QSignalBlocker blocker(m_ruleCombo);
        m_ruleCombo->clear();
        m_ruleCombo->addItems(names);
        m_ruleCombo->setCurrentIndex(index);
    }
    m_syncing = false;
}

void ControlPanel::syncBoundarySelection()
{
    const QSignalBlocker blocker(m_boundaryCombo);
    if (m_boundaryCombo->count() == 0)
    {
        m_boundaryCombo->addItems({tr("死亡边界"), tr("环绕边界")});
    }
    m_boundaryCombo->setCurrentIndex(m_boundary == domain::Boundary::Wrap ? 1 : 0);
}

void ControlPanel::updateJumpState()
{
    if (m_rewindButton == nullptr || m_forwardButton == nullptr || m_jumpSpin == nullptr)
    {
        return;
    }

    const int generations = m_jumpSpin->value();
    const bool canRewind = m_currentGeneration > m_oldestGeneration;
    m_rewindButton->setEnabled(canRewind);
    if (canRewind)
    {
        m_rewindButton->setToolTip(tr("回退 %1 代（最早到第 %2 代）")
                                       .arg(QLocale().toString(generations))
                                       .arg(QLocale().toString(m_oldestGeneration)));
    }
    else
    {
        m_rewindButton->setToolTip(
            tr("已是可回退的最早一代（第 %1 代）").arg(QLocale().toString(m_oldestGeneration)));
    }
    m_forwardButton->setToolTip(tr("快进 %1 代（不逐代刷新界面）").arg(QLocale().toString(generations)));
}

void ControlPanel::updateSpeedLabel()
{
    m_speedValue->setText(tr("%1 代/秒").arg(m_speed));
    m_speedValue->setToolTip(m_speed == 0 ? tr("速度为 0：模拟暂停") : tr("每秒演化代数"));
}

} // namespace lifegame::view
