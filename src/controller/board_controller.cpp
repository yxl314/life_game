#include "controller/board_controller.h"

#include <QAction>
#include <QPoint>
#include <QSignalBlocker>

#include <algorithm>

#include "controller/error_text.h"
#include "controller/line_raster.h"
#include "domain/pattern_library.h"
#include "model/life_model.h"
#include "shared/app_settings.h"
#include "view/board_view.h"
#include "view/control_panel.h"
#include "view/interfaces.h"
#include "view/pattern_catalog.h"

namespace lifegame::controller
{

BoardController::BoardController(model::LifeModel* model,
                                 view::BoardView* view,
                                 view::ControlPanel* panel,
                                 const view::PatternCatalog* catalog,
                                 view::MessageSink* messages,
                                 view::ConfirmSink* confirm,
                                 QObject* parent)
    : QObject(parent),
      m_model(model),
      m_view(view),
      m_panel(panel),
      m_catalog(catalog),
      m_messages(messages),
      m_confirm(confirm)
{
    connect(m_view, &view::BoardView::cellPressed, this, &BoardController::onCellPressed);
    connect(m_view, &view::BoardView::cellDragged, this, &BoardController::onCellDragged);
    connect(m_view, &view::BoardView::cellReleased, this, &BoardController::onCellReleased);
    connect(m_panel, &view::ControlPanel::patternChanged, this, &BoardController::selectPattern);
    connect(m_panel, &view::ControlPanel::clearRequested, this, &BoardController::clearBoard);
    connect(m_panel, &view::ControlPanel::randomizeRequested, this, &BoardController::onPanelRandomize);
    connect(m_panel, &view::ControlPanel::randomDensityChanged, this, &BoardController::setRandomDensity);
    connect(m_panel, &view::ControlPanel::resizeRequested, this, &BoardController::resizeBoard);
    connect(m_panel, &view::ControlPanel::gridVisibilityChanged, this, &BoardController::setGridVisible);

    QVector<view::PatternOption> options;
    options.append({tr("画笔（直接绘制）"), QString()});
    for (const domain::Pattern& pattern : domain::builtinPatterns())
    {
        options.append({m_catalog->displayLabel(pattern), QString::fromStdString(pattern.id())});
    }
    m_panel->setPatternOptions(options);
    selectPattern(QString());

    m_gridVisible = shared::AppSettings{}.gridVisible;
    m_density = shared::AppSettings{}.density;
    m_sizeStep = shared::AppSettings{}.sizeStep;
    m_view->setGridVisible(m_gridVisible);
    m_panel->setGridVisible(m_gridVisible);
    m_panel->setRandomDensity(m_density);
    m_panel->setSizeStep(m_sizeStep);
}

int BoardController::randomDensity() const
{
    return m_density;
}

void BoardController::setRandomDensity(int density)
{
    m_density =
        std::clamp(density, shared::limits::kDensityMin, shared::limits::kDensityMax);
    m_panel->setRandomDensity(m_density);
}

int BoardController::sizeStep() const
{
    return m_sizeStep;
}

void BoardController::setSizeStep(int step)
{
    m_sizeStep = std::clamp(step, shared::limits::kSizeStepMin, shared::limits::kSizeStepMax);
    m_panel->setSizeStep(m_sizeStep);
}

void BoardController::setGridAction(QAction* action)
{
    m_gridAction = action;
    if (m_gridAction != nullptr)
    {
        const QSignalBlocker blocker(m_gridAction);
        m_gridAction->setChecked(m_gridVisible);
    }
}

bool BoardController::gridVisible() const
{
    return m_gridVisible;
}

void BoardController::setGridVisible(bool visible)
{
    m_gridVisible = visible;
    m_view->setGridVisible(visible);
    m_panel->setGridVisible(visible);
    if (m_gridAction != nullptr)
    {
        const QSignalBlocker blocker(m_gridAction);
        m_gridAction->setChecked(visible);
    }
}

void BoardController::onCellPressed(int row, int col)
{
    if (m_model->isJumping())
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("跳转进行中，暂不能编辑棋盘。"), 3000);
        }
        return;
    }

    if (m_stampMode)
    {
        applyStamp(row, col);
        return;
    }

    m_paintValue = !m_model->board().cell(row, col);
    m_painting = true;
    m_lastRow = row;
    m_lastCol = col;
    m_model->setCell(row, col, m_paintValue);
}

void BoardController::onCellDragged(int row, int col)
{
    if (!m_painting || m_stampMode || m_model->isJumping())
    {
        return;
    }
    if (row == m_lastRow && col == m_lastCol)
    {
        return;
    }

    if (m_lastRow >= 0 && m_lastCol >= 0)
    {
        paintLine(m_lastRow, m_lastCol, row, col, m_paintValue);
    }
    else
    {
        m_model->setCell(row, col, m_paintValue);
    }

    m_lastRow = row;
    m_lastCol = col;
}

void BoardController::onCellReleased(int row, int col)
{
    Q_UNUSED(row)
    Q_UNUSED(col)
    m_painting = false;
    m_lastRow = -1;
    m_lastCol = -1;
}

void BoardController::paintLine(int fromRow, int fromCol, int toRow, int toCol, bool alive)
{
    const std::vector<CellPosition> positions = rasterLine(fromRow, fromCol, toRow, toCol);
    std::vector<std::pair<int, int>> cells;
    cells.reserve(positions.size());
    for (const CellPosition& position : positions)
    {
        cells.emplace_back(position.row, position.col);
    }
    m_model->setCells(cells, alive);
}

void BoardController::applyStamp(int row, int col)
{
    const model::CommandResult result = m_model->placePattern(m_stamp, row, col);
    if (m_messages == nullptr)
    {
        return;
    }
    if (result == model::CommandResult::Success || result == model::CommandResult::Clipped)
    {
        QString text = tr("已放置图案：%1").arg(m_catalog->displayLabel(m_stamp));
        if (result == model::CommandResult::Clipped)
        {
            text += tr("（越界部分已裁掉）");
        }
        m_messages->showMessage(text, 3000);
    }
    else if (result == model::CommandResult::NoChange)
    {
        m_messages->showMessage(tr("图案没有改变任何细胞。"), 3000);
    }
    else
    {
        m_messages->showMessage(commandResultText(result), 4000);
    }
}

void BoardController::selectPattern(const QString& patternId)
{
    const domain::Pattern* pattern = domain::findBuiltinPattern(patternId.toStdString());
    if (pattern == nullptr)
    {
        m_stampMode = false;
        m_stamp = domain::Pattern();
        m_view->setStampPreview({});
        m_panel->setPatternDescription(tr("画笔模式：左键点击切换细胞，按住拖动可连续绘制或擦除。"));
        return;
    }

    m_stamp = *pattern;
    m_stampMode = true;

    QVector<QPoint> offsets;
    offsets.reserve(m_stamp.population());
    const int centerCol = m_stamp.width() / 2;
    const int centerRow = m_stamp.height() / 2;
    for (int row = 0; row < m_stamp.height(); ++row)
    {
        for (int col = 0; col < m_stamp.width(); ++col)
        {
            if (m_stamp.alive(row, col))
            {
                offsets.append(QPoint(col - centerCol, row - centerRow));
            }
        }
    }
    m_view->setStampPreview(offsets);
    m_panel->setPatternDescription(tr("%1\n%2\n%3 × %4 · 点击棋盘放置，可连续放置。")
                                       .arg(m_catalog->displayLabel(m_stamp),
                                            m_catalog->descriptionOf(m_stamp.id()))
                                       .arg(m_stamp.width())
                                       .arg(m_stamp.height()));
}

void BoardController::clearBoard()
{
    if (m_model->stats().population > 0 && m_confirm != nullptr &&
        !m_confirm->confirm(tr("清空棋盘"), tr("确定要清空当前棋盘吗？此操作无法撤销。")))
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("已取消清空。"), 3000);
        }
        return;
    }

    const model::CommandResult result = m_model->clear();
    if (m_messages == nullptr)
    {
        return;
    }
    m_messages->showMessage(result == model::CommandResult::Success ? tr("棋盘已清空。")
                                                                   : commandResultText(result),
                            3000);
}

void BoardController::randomizeBoard()
{
    onPanelRandomize(m_density);
}

void BoardController::onPanelRandomize(int density)
{
    setRandomDensity(density);
    const model::CommandResult result = m_model->randomize(m_density);
    if (m_messages == nullptr)
    {
        return;
    }
    if (result == model::CommandResult::Success)
    {
        m_messages->showMessage(tr("已按 %1% 密度随机填充。").arg(density), 4000);
    }
    else
    {
        m_messages->showMessage(commandResultText(result), 4000);
    }
}

void BoardController::resizeBoard(int width, int height)
{
    if (m_model->stats().population > 0 && m_confirm != nullptr &&
        !m_confirm->confirm(tr("调整棋盘尺寸"), tr("调整尺寸会清空当前棋盘，是否继续？")))
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(tr("已取消调整尺寸。"), 3000);
        }
        return;
    }

    const bool wasEmpty = m_model->stats().population == 0;
    const model::CommandResult result = m_model->resizeBoard(width, height);
    if (m_messages == nullptr)
    {
        return;
    }
    if (result == model::CommandResult::Success || result == model::CommandResult::Adjusted)
    {
        QString text = tr("棋盘已调整为 %1 × %2%3。")
                           .arg(m_model->width())
                           .arg(m_model->height())
                           .arg(wasEmpty ? QString() : tr("（原棋盘已清空）"));
        if (result == model::CommandResult::Adjusted)
        {
            text += tr("（尺寸已限制到 20–1500 × 20–1000）");
        }
        m_messages->showMessage(text, 4000);
    }
    else
    {
        m_messages->showMessage(commandResultText(result), 4000);
    }
}

void BoardController::zoomIn()
{
    m_view->zoomIn();
}

void BoardController::zoomOut()
{
    m_view->zoomOut();
}

void BoardController::resetZoom()
{
    m_view->resetZoom();
}

void BoardController::loadDemo()
{
    const domain::Pattern* gun = domain::findBuiltinPattern("gosper_gun");
    const domain::Pattern* glider = domain::findBuiltinPattern("glider");
    const domain::Pattern* chosen = glider;
    if (gun != nullptr && m_model->width() >= 48 && m_model->height() >= 24)
    {
        chosen = gun;
    }
    if (chosen == nullptr)
    {
        return;
    }
    const model::CommandResult cleared = m_model->clear();
    if (cleared != model::CommandResult::Success)
    {
        if (m_messages != nullptr)
        {
            m_messages->showMessage(commandResultText(cleared), 4000);
        }
        return;
    }

    const model::CommandResult placed = m_model->placePattern(*chosen, m_model->height() / 2, m_model->width() / 2);
    if (m_messages != nullptr)
    {
        if (model::succeeded(placed))
        {
            m_messages->showMessage(tr("已载入示例图案，按空格或点击“开始”运行。"), 5000);
        }
        else
        {
            m_messages->showMessage(commandResultText(placed), 4000);
        }
    }
}

} // namespace lifegame::controller
