#include "view/board_view.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtMath>

#include <algorithm>
#include <cmath>

#include "domain/grid.h"
#include "shared/app_settings.h"
#include "domain/grid.h"
#include "shared/app_settings.h"

namespace lifegame::view
{

namespace
{

constexpr double kMargin = 24.0;
constexpr double kMinCellPixels = 1.0;
constexpr double kMaxCellPixels = 48.0;
constexpr double kCellImageThreshold = 6.0;
constexpr double kGridCacheMaxPixels = 4.0 * 1024.0 * 1024.0;
constexpr int kExportCellPixels = 8;
constexpr int kExportMaxDimension = 4096;
constexpr int kExportMargin = 16;

const QColor kBackground(11, 15, 20);
const QColor kBoardBackground(17, 22, 28);
const QColor kBorder(48, 54, 61);
const QColor kGrid(38, 46, 56);
const QColor kAlive(63, 185, 80);
const QColor kAliveHover(94, 220, 110);
const QColor kHover(88, 166, 255);
const QColor kPreview(210, 153, 34);
const QColor kHint(125, 133, 144);

QRectF cellRectAt(const QPointF& topLeft, int row, int col, double cellPixels, double gap)
{
    return QRectF(topLeft.x() + static_cast<double>(col) * cellPixels + gap / 2.0,
                  topLeft.y() + static_cast<double>(row) * cellPixels + gap / 2.0,
                  std::max(1.0, cellPixels - gap),
                  std::max(1.0, cellPixels - gap));
}

void drawCellShape(QPainter& painter, const QRectF& rect, double cellPixels, double radius)
{
    if (cellPixels >= kCellImageThreshold)
    {
        painter.drawRoundedRect(rect, radius, radius);
    }
    else
    {
        painter.drawRect(rect);
    }
}

} // namespace

BoardView::BoardView(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    setMinimumSize(420, 320);
    setToolTip(tr("左键绘制 / 擦除\n中键或右键拖动平移\n滚轮缩放"));
}

void BoardView::applyFrame(const BoardFrame& frame)
{
    const bool boardChanged = m_board != frame.board;
    m_board = frame.board;
    m_population = std::max(0, frame.population);
    m_animating = frame.running;
    if (boardChanged)
    {
        m_hover_row = -1;
        m_hover_col = -1;
        m_cell_pixels = fitCellPixels();
        m_pan = QPointF();
    }
    update();
}

void BoardView::refresh()
{
    update();
}

QSize BoardView::sizeHint() const
{
    return QSize(900, 640);
}

QSize BoardView::minimumSizeHint() const
{
    return QSize(420, 320);
}

bool BoardView::gridVisible() const
{
    return m_grid_visible;
}

void BoardView::setStampPreview(const QVector<QPoint>& offsets)
{
    m_preview = offsets;
    update();
}

void BoardView::setGridVisible(bool visible)
{
    if (m_grid_visible == visible)
    {
        return;
    }
    m_grid_visible = visible;
    m_grid_pixmap = QPixmap();
    update();
}

void BoardView::zoomIn()
{
    zoomBy(1.2);
}

void BoardView::zoomOut()
{
    zoomBy(1.0 / 1.2);
}

void BoardView::resetZoom()
{
    m_auto_fit = true;
    m_pan = QPointF();
    m_cell_pixels = fitCellPixels();
    update();
}

int BoardView::boardWidth() const
{
    return m_board != nullptr ? m_board->width() : 0;
}

int BoardView::boardHeight() const
{
    return m_board != nullptr ? m_board->height() : 0;
}

double BoardView::cellPixels() const
{
    return m_auto_fit ? fitCellPixels() : m_cell_pixels;
}

double BoardView::fitCellPixels() const
{
    const int width = boardWidth();
    const int height = boardHeight();
    if (width <= 0 || height <= 0)
    {
        return 8.0;
    }

    const double availableWidth = std::max(1.0, static_cast<double>(this->width()) - 2.0 * kMargin);
    const double availableHeight = std::max(1.0, static_cast<double>(this->height()) - 2.0 * kMargin);
    const double fit = std::min(availableWidth / static_cast<double>(width),
                                availableHeight / static_cast<double>(height));
    return std::max(kMinCellPixels, std::min(32.0, fit));
}

QPointF BoardView::boardTopLeft() const
{
    const double pixels = cellPixels();
    const double boardWidthPixels = static_cast<double>(boardWidth()) * pixels;
    const double boardHeightPixels = static_cast<double>(boardHeight()) * pixels;
    return QPointF((static_cast<double>(width()) - boardWidthPixels) / 2.0 + m_pan.x(),
                   (static_cast<double>(height()) - boardHeightPixels) / 2.0 + m_pan.y());
}

bool BoardView::cellFromPosition(const QPoint& position, int* row, int* col) const
{
    const double pixels = cellPixels();
    const int width = boardWidth();
    const int height = boardHeight();
    if (width <= 0 || height <= 0 || pixels <= 0.0)
    {
        return false;
    }

    const QPointF top = boardTopLeft();
    const double x = static_cast<double>(position.x()) - top.x();
    const double y = static_cast<double>(position.y()) - top.y();
    if (x < 0.0 || y < 0.0)
    {
        return false;
    }

    const int c = static_cast<int>(std::floor(x / pixels));
    const int r = static_cast<int>(std::floor(y / pixels));
    if (r < 0 || r >= height || c < 0 || c >= width)
    {
        return false;
    }

    *row = r;
    *col = c;
    return true;
}

void BoardView::clampPan()
{
    const double pixels = cellPixels();
    const double boardWidthPixels = static_cast<double>(boardWidth()) * pixels;
    const double boardHeightPixels = static_cast<double>(boardHeight()) * pixels;

    const double slackX = std::max(0.0, (boardWidthPixels - static_cast<double>(width())) / 2.0) + 80.0;
    const double slackY = std::max(0.0, (boardHeightPixels - static_cast<double>(height())) / 2.0) + 80.0;

    m_pan.setX(std::max(-slackX, std::min(slackX, m_pan.x())));
    m_pan.setY(std::max(-slackY, std::min(slackY, m_pan.y())));
}

void BoardView::zoomBy(double factor)
{
    zoomAt(QPointF(static_cast<double>(width()) / 2.0, static_cast<double>(height()) / 2.0), factor);
}

void BoardView::zoomAt(const QPointF& anchor, double factor)
{
    if (boardWidth() <= 0 || boardHeight() <= 0 || factor <= 0.0)
    {
        return;
    }

    const double oldPixels = cellPixels();
    const double newPixels = std::max(kMinCellPixels, std::min(kMaxCellPixels, oldPixels * factor));
    if (std::abs(newPixels - oldPixels) < 0.001)
    {
        return;
    }

    const QPointF oldTop = boardTopLeft();
    const double anchorX = (anchor.x() - oldTop.x()) / oldPixels;
    const double anchorY = (anchor.y() - oldTop.y()) / oldPixels;

    m_auto_fit = false;
    m_cell_pixels = newPixels;

    const QPointF centered((static_cast<double>(width()) - static_cast<double>(boardWidth()) * newPixels) / 2.0,
                           (static_cast<double>(height()) - static_cast<double>(boardHeight()) * newPixels) / 2.0);
    m_pan = QPointF(anchor.x() - anchorX * newPixels, anchor.y() - anchorY * newPixels) -
            centered;

    clampPan();
    update();
}

std::optional<VisibleCellRange> BoardView::visibleCellRange(const QRectF& boardRect,
                                                            double cellPixelsValue) const
{
    if (cellPixelsValue <= 0.0 || boardWidth() <= 0 || boardHeight() <= 0)
    {
        return std::nullopt;
    }

    const QRectF visible = boardRect.intersected(QRectF(rect()));
    if (visible.isEmpty())
    {
        return std::nullopt;
    }

    VisibleCellRange range;
    range.firstRow = std::clamp(
        static_cast<int>(std::floor((visible.top() - boardRect.top()) / cellPixelsValue)), 0, boardHeight() - 1);
    range.lastRow = std::clamp(
        static_cast<int>(std::ceil((visible.bottom() - boardRect.top()) / cellPixelsValue)) - 1,
        0,
        boardHeight() - 1);
    range.firstCol = std::clamp(
        static_cast<int>(std::floor((visible.left() - boardRect.left()) / cellPixelsValue)), 0, boardWidth() - 1);
    range.lastCol = std::clamp(
        static_cast<int>(std::ceil((visible.right() - boardRect.left()) / cellPixelsValue)) - 1,
        0,
        boardWidth() - 1);

    if (range.firstRow > range.lastRow || range.firstCol > range.lastCol)
    {
        return std::nullopt;
    }
    return range;
}

void BoardView::drawBoard(QPainter& painter,
                          const domain::Grid& board,
                          const QPointF& topLeft,
                          double cellPixelsValue,
                          bool interactive) const
{
    const int width = board.width();
    const int height = board.height();
    if (width <= 0 || height <= 0)
    {
        return;
    }

    const QRectF boardRect(topLeft,
                           QSizeF(static_cast<double>(width) * cellPixelsValue,
                                  static_cast<double>(height) * cellPixelsValue));

    const double gap = cellPixelsValue >= 7.0 ? 1.0 : (cellPixelsValue >= 4.0 ? 0.5 : 0.0);
    const double radius = std::min(2.5, cellPixelsValue * 0.2);

    VisibleCellRange range{0, height - 1, 0, width - 1};
    if (interactive)
    {
        const std::optional<VisibleCellRange> visible = visibleCellRange(boardRect, cellPixelsValue);
        if (!visible.has_value())
        {
            return;
        }
        range = *visible;
    }

    const bool tooManyCells = width * height > shared::limits::kLargeBoardCellThreshold;
    if ((m_animating && interactive) || cellPixelsValue < kCellImageThreshold || tooManyCells)
    {
        drawCellsAsImage(painter, board, boardRect, interactive, range);
    }
    else
    {
        painter.fillRect(boardRect, kBoardBackground);
        drawCellsDetailed(painter,
                          board,
                          topLeft,
                          cellPixelsValue,
                          gap,
                          radius,
                          interactive,
                          range);
    }

    if (interactive && !m_preview.isEmpty() && m_hover_row >= 0 && m_hover_col >= 0)
    {
        QColor fill = kPreview;
        fill.setAlpha(90);
        QColor stroke = kPreview;
        stroke.setAlpha(210);

        for (const QPoint& offset : m_preview)
        {
            const int row = m_hover_row + offset.y();
            const int col = m_hover_col + offset.x();
            if (row < 0 || row >= height || col < 0 || col >= width)
            {
                continue;
            }

            if (cellPixelsValue >= kCellImageThreshold)
            {
                painter.setPen(QPen(stroke, 1.0));
            }
            else
            {
                painter.setPen(Qt::NoPen);
            }
            painter.setBrush(fill);
            drawCellShape(painter, cellRectAt(topLeft, row, col, cellPixelsValue, gap), cellPixelsValue,
                          radius);
        }
    }

    if (m_grid_visible && cellPixelsValue >= 3.0)
    {
        const double dpr = devicePixelRatioF();
        const double cachePixels = boardRect.width() * boardRect.height() * dpr * dpr;
        const bool fullBoardVisible = QRectF(rect()).contains(boardRect);
        if (cachePixels <= kGridCacheMaxPixels && fullBoardVisible)
        {
            updateGridPixmap(boardRect, cellPixelsValue);
            painter.drawPixmap(boardRect.topLeft(), m_grid_pixmap);
        }
        else
        {
            drawGridLinesRange(painter,
                               boardRect,
                               cellPixelsValue,
                               1,
                               range);
        }
    }
    else if (m_grid_visible && cellPixelsValue > 0.0)
    {
        // At large boards the per-cell grid would be sub-pixel and invisible.
        // Draw a coarse grid every N cells instead of pretending the toggle
        // did nothing.
        const int stride = std::max(1, static_cast<int>(std::ceil(3.0 / cellPixelsValue)));
        drawGridLinesRange(painter,
                           boardRect,
                           cellPixelsValue,
                           stride,
                           range);
    }

    if (interactive && m_hover_row >= 0 && m_hover_col >= 0)
    {
        QColor outline = m_preview.isEmpty() ? kHover : kPreview;
        outline.setAlpha(150);
        QPen pen(outline);
        pen.setWidthF(1.0);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(QRectF(topLeft.x() + static_cast<double>(m_hover_col) * cellPixelsValue + 0.5,
                                topLeft.y() + static_cast<double>(m_hover_row) * cellPixelsValue + 0.5,
                                std::max(1.0, cellPixelsValue - 1.0),
                                std::max(1.0, cellPixelsValue - 1.0)));
    }

    painter.setPen(QPen(kBorder, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(boardRect.adjusted(-0.5, -0.5, 0.5, 0.5));
}

void BoardView::drawGridLines(QPainter& painter,
                              const QRectF& boardRect,
                              double cellPixelsValue,
                              int stride) const
{
    drawGridLinesRange(painter, boardRect, cellPixelsValue, stride, VisibleCellRange{0, boardHeight() - 1, 0, boardWidth() - 1});
}

void BoardView::drawGridLinesRange(QPainter& painter,
                                   const QRectF& boardRect,
                                   double cellPixelsValue,
                                   int stride,
                                   const VisibleCellRange& range) const
{
    painter.setPen(QPen(kGrid, 1.0));
    painter.setBrush(Qt::NoBrush);

    const int step = std::max(1, stride);
    const int startCol = (range.firstCol / step) * step;
    const int startRow = (range.firstRow / step) * step;

    for (int col = startCol; col <= range.lastCol + 1; col += step)
    {
        const double x = boardRect.left() + static_cast<double>(col) * cellPixelsValue;
        painter.drawLine(QPointF(x, boardRect.top()), QPointF(x, boardRect.bottom()));
    }
    for (int row = startRow; row <= range.lastRow + 1; row += step)
    {
        const double y = boardRect.top() + static_cast<double>(row) * cellPixelsValue;
        painter.drawLine(QPointF(boardRect.left(), y), QPointF(boardRect.right(), y));
    }
}

void BoardView::updateGridPixmap(const QRectF& boardRect, double cellPixelsValue) const
{
    const qreal dpr = devicePixelRatioF();
    const QSize device(qCeil(boardRect.width() * dpr), qCeil(boardRect.height() * dpr));

    if (m_grid_pixmap.size() == device && m_grid_key == boardRect && m_grid_cell == cellPixelsValue &&
        m_grid_dpr == dpr)
    {
        return;
    }

    m_grid_pixmap = QPixmap(device);
    m_grid_pixmap.setDevicePixelRatio(dpr);
    m_grid_pixmap.fill(Qt::transparent);

    QPainter painter(&m_grid_pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawGridLines(painter, QRectF(0.0, 0.0, boardRect.width(), boardRect.height()), cellPixelsValue);

    m_grid_key = boardRect;
    m_grid_cell = cellPixelsValue;
    m_grid_dpr = dpr;
}

void BoardView::drawCellsDetailed(QPainter& painter,
                                  const domain::Grid& board,
                                  const QPointF& topLeft,
                                  double cellPixelsValue,
                                  double gap,
                                  double radius,
                                  bool interactive,
                                  const VisibleCellRange& range) const
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(kAlive);

    const int width = board.width();
    const domain::Cell* cells = board.data();

    const int hoverRow = interactive ? m_hover_row : -1;
    const int hoverCol = interactive ? m_hover_col : -1;

    for (int row = range.firstRow; row <= range.lastRow; ++row)
    {
        const std::size_t base = static_cast<std::size_t>(row) * static_cast<std::size_t>(width);
        for (int col = range.firstCol; col <= range.lastCol; ++col)
        {
            if (cells[base + static_cast<std::size_t>(col)] == domain::kDead)
            {
                continue;
            }
            if (row == hoverRow && col == hoverCol)
            {
                continue;
            }

            drawCellShape(painter,
                          cellRectAt(topLeft, row, col, cellPixelsValue, gap),
                          cellPixelsValue,
                          radius);
        }
    }

    if (hoverRow >= range.firstRow && hoverRow <= range.lastRow && hoverCol >= range.firstCol &&
        hoverCol <= range.lastCol)
    {
        painter.setBrush(kAliveHover);
        drawCellShape(painter,
                      cellRectAt(topLeft, hoverRow, hoverCol, cellPixelsValue, gap),
                      cellPixelsValue,
                      radius);
    }
}

void BoardView::drawCellsAsImage(QPainter& painter,
                                 const domain::Grid& board,
                                 const QRectF& boardRect,
                                 bool interactive,
                                 const VisibleCellRange& range) const
{
    const int width = board.width();
    const int cols = range.lastCol - range.firstCol + 1;
    const int rows = range.lastRow - range.firstRow + 1;
    if (width <= 0 || cols <= 0 || rows <= 0)
    {
        return;
    }

    if (m_cell_image.size() != QSize(cols, rows))
    {
        m_cell_image = QImage(cols, rows, QImage::Format_ARGB32_Premultiplied);
    }

    const QRgb alive = kAlive.rgba();
    const QRgb hover = kAliveHover.rgba();
    const QRgb dead = kBoardBackground.rgb();
    const domain::Cell* cells = board.data();
    const int hoverRow = interactive ? m_hover_row : -1;
    const int hoverCol = interactive ? m_hover_col : -1;

    for (int row = 0; row < rows; ++row)
    {
        QRgb* line = reinterpret_cast<QRgb*>(m_cell_image.scanLine(row));
        const int sourceRow = range.firstRow + row;
        const int base = sourceRow * width + range.firstCol;
        for (int col = 0; col < cols; ++col)
        {
            line[col] = cells[base + col] != domain::kDead ? alive : dead;
        }
        if (sourceRow == hoverRow && hoverCol >= range.firstCol && hoverCol <= range.lastCol &&
            cells[sourceRow * width + hoverCol] != domain::kDead)
        {
            line[hoverCol - range.firstCol] = hover;
        }
    }

    const QRectF target(boardRect.left() + static_cast<double>(range.firstCol) * (boardRect.width() / width),
                        boardRect.top() + static_cast<double>(range.firstRow) * (boardRect.height() / board.height()),
                        static_cast<double>(cols) * (boardRect.width() / width),
                        static_cast<double>(rows) * (boardRect.height() / board.height()));
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::NoBrush);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(target, m_cell_image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

void BoardView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)

    QElapsedTimer paintTimer;
    paintTimer.start();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), kBackground);

    if (m_board == nullptr || boardWidth() <= 0 || boardHeight() <= 0)
    {
        return;
    }

    drawBoard(painter, *m_board, boardTopLeft(), cellPixels(), true);

    if (m_population == 0 && m_preview.isEmpty())
    {
        painter.setPen(kHint);
        QFont font = painter.font();
        font.setPointSize(12);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter, tr("点击或拖动绘制细胞\n或使用「随机填充」"));
    }
    painter.end();
    m_last_paint_ms = static_cast<double>(paintTimer.nsecsElapsed()) / 1.0e6;
}

QImage BoardView::renderBoardImage() const
{
    if (m_board == nullptr || boardWidth() <= 0 || boardHeight() <= 0)
    {
        return QImage();
    }

    const int width = boardWidth();
    const int height = boardHeight();

    // Export resolution is independent of the 1..48 px/zoom level: an export
    // at 48 px/cell on a large board would need many GiB.
    int pixels = kExportCellPixels;
    while (pixels > 1 &&
           (width * pixels + 2 * kExportMargin > kExportMaxDimension ||
            height * pixels + 2 * kExportMargin > kExportMaxDimension))
    {
        --pixels;
    }

    QImage image(width * pixels + 2 * kExportMargin,
                 height * pixels + 2 * kExportMargin,
                 QImage::Format_ARGB32_Premultiplied);
    if (image.isNull())
    {
        return image;
    }
    image.fill(kBackground);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    drawBoard(painter, *m_board, QPointF(kExportMargin, kExportMargin), static_cast<double>(pixels), false);
    painter.end();
    return image;
}

void BoardView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_auto_fit)
    {
        m_cell_pixels = fitCellPixels();
        m_pan = QPointF();
    }
    else
    {
        clampPan();
    }
}

void BoardView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton)
    {
        m_panning = true;
        m_last_mouse = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton)
    {
        int row = -1;
        int col = -1;
        if (cellFromPosition(event->pos(), &row, &col))
        {
            m_left_pressed = true;
            emit cellPressed(row, col);
            event->accept();
            return;
        }
    }

    QWidget::mousePressEvent(event);
}

void BoardView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning)
    {
        const QPoint delta = event->pos() - m_last_mouse;
        m_last_mouse = event->pos();
        m_pan += QPointF(delta);
        clampPan();
        update();
        event->accept();
        return;
    }

    int row = -1;
    int col = -1;
    const bool inside = cellFromPosition(event->pos(), &row, &col);
    if (!inside)
    {
        row = -1;
        col = -1;
    }

    if (row != m_hover_row || col != m_hover_col)
    {
        m_hover_row = row;
        m_hover_col = col;
        update();
    }

    if (m_left_pressed && inside)
    {
        emit cellDragged(row, col);
    }

    QWidget::mouseMoveEvent(event);
}

void BoardView::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_panning && (event->button() == Qt::MiddleButton || event->button() == Qt::RightButton))
    {
        m_panning = false;
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && m_left_pressed)
    {
        m_left_pressed = false;

        int row = -1;
        int col = -1;
        cellFromPosition(event->pos(), &row, &col);
        emit cellReleased(row, col);
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void BoardView::wheelEvent(QWheelEvent* event)
{
    if (boardWidth() <= 0 || boardHeight() <= 0)
    {
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0)
    {
        return;
    }

    const double factor = delta > 0 ? 1.15 : 1.0 / 1.15;
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    const QPoint position = event->position().toPoint();
#else
    const QPoint position = event->pos();
#endif
    zoomAt(QPointF(position), factor);
    event->accept();
}

void BoardView::leaveEvent(QEvent* event)
{
    if (m_hover_row != -1 || m_hover_col != -1)
    {
        m_hover_row = -1;
        m_hover_col = -1;
        update();
    }
    QWidget::leaveEvent(event);
}

} // namespace lifegame::view
