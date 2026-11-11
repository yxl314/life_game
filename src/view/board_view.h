#pragma once

/**
 * @file board_view.h
 * @brief Custom-painted passive board view.
 */

#include <QElapsedTimer>

#include <optional>
#include <QImage>
#include <QPixmap>
#include <QPoint>
#include <QRectF>
#include <QVector>
#include <QWidget>

#include "view/interfaces.h"
#include "view/view_state.h"

namespace lifegame::domain
{
class Grid;
}

namespace lifegame::view
{

/**
 * @brief Custom-painted, passive board view.
 *
 * Layered MVC passive view: the controller pushes a `const domain::Grid*`
 * plus a little render state. The view never includes or observes the Model,
 * never copies the board and never calls a mutating model method.
 */
class BoardView : public QWidget, public BoardImageSource, public FrameTimingSource
{
    Q_OBJECT

public:
    /// @param parent Parent widget.
    explicit BoardView(QWidget* parent = nullptr);

    /// @brief Request a repaint.
    void refresh();

    /// @brief Atomically apply one complete board frame.
    /// @param frame Board pointer, population and running state.
    void applyFrame(const BoardFrame& frame);

    /// @return Duration of the last paint in milliseconds (0 before first paint).
    double lastPaintDurationMs() const override
    {
        return m_last_paint_ms;
    }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /// @return True when grid lines are visible.
    bool gridVisible() const;

    /// @return Full-board PNG-ready image (export path).
    QImage renderBoardImage() const override;

public slots:
    /// @param offsets Pattern offsets relative to the hover cell.
    void setStampPreview(const QVector<QPoint>& offsets);

    /// @param visible Whether grid lines are shown.
    void setGridVisible(bool visible);

    /// @brief Zoom in one step.
    void zoomIn();

    /// @brief Zoom out one step.
    void zoomOut();

    /// @brief Return to auto-fit zoom and centered pan.
    void resetZoom();

signals:
    /// @brief Primary mouse button pressed on a cell.
    /// @param row Cell row.
    /// @param col Cell column.
    void cellPressed(int row, int col);

    /// @brief Primary button dragged over a cell.
    /// @param row Cell row.
    /// @param col Cell column.
    void cellDragged(int row, int col);

    /// @brief Primary button released over a cell.
    /// @param row Cell row.
    /// @param col Cell column.
    void cellReleased(int row, int col);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int boardWidth() const;
    int boardHeight() const;

    double cellPixels() const;
    QPointF boardTopLeft() const;
    double fitCellPixels() const;
    bool cellFromPosition(const QPoint& position, int* row, int* col) const;
    std::optional<VisibleCellRange> visibleCellRange(const QRectF& boardRect,
                                                     double cellPixels) const;
    void clampPan();
    void zoomBy(double factor);
    void zoomAt(const QPointF& anchor, double factor);

    void drawBoard(QPainter& painter,
                   const domain::Grid& board,
                   const QPointF& topLeft,
                   double cellPixels,
                   bool interactive) const;
    void drawCellsDetailed(QPainter& painter,
                           const domain::Grid& board,
                           const QPointF& topLeft,
                           double cellPixels,
                           double gap,
                           double radius,
                           bool interactive,
                           const VisibleCellRange& range) const;
    void drawCellsAsImage(QPainter& painter,
                          const domain::Grid& board,
                          const QRectF& boardRect,
                          bool interactive,
                          const VisibleCellRange& range) const;
    void updateGridPixmap(const QRectF& boardRect, double cellPixels) const;
    void drawGridLines(QPainter& painter, const QRectF& boardRect, double cellPixels, int stride = 1) const;
    void drawGridLinesRange(QPainter& painter,
                            const QRectF& boardRect,
                            double cellPixels,
                            int stride,
                            const VisibleCellRange& range) const;

    const domain::Grid* m_board = nullptr;
    int m_population = 0;

    QVector<QPoint> m_preview;
    bool m_grid_visible = true;
    bool m_animating = false;

    mutable QImage m_cell_image;
    mutable QPixmap m_grid_pixmap;
    mutable QRectF m_grid_key;
    mutable double m_grid_cell = 0.0;
    mutable qreal m_grid_dpr = 0.0;

    double m_cell_pixels = 8.0;
    bool m_auto_fit = true;
    QPointF m_pan;

    double m_last_paint_ms = 0.0;
    int m_hover_row = -1;
    int m_hover_col = -1;
    bool m_left_pressed = false;
    bool m_panning = false;
    QPoint m_last_mouse;
};

} // namespace lifegame::view
