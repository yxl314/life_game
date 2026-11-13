#pragma once

/**
 * @file board_view.h
 * @brief 自绘的被动棋盘视图。
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
 * @brief 自绘的被动棋盘视图。
 *
 * 分层 MVC 中的被动视图：控制器推入 `const domain::Grid*` 和少量渲染状态。
 * 视图不包含也不观察模型，不复制棋盘，更不调用会修改模型的方法。
 */
class BoardView : public QWidget, public BoardImageSource, public FrameTimingSource
{
    Q_OBJECT

public:
    /// @brief 构造棋盘视图。
    /// @param parent 父控件。
    explicit BoardView(QWidget* parent = nullptr);

    /// @brief 一次性应用一整帧棋盘状态。
    /// @param frame 棋盘指针、存活数与运行状态。
    void applyFrame(const BoardFrame& frame);

    /// @brief 返回上一帧的绘制耗时。
    /// @return 毫秒数；首次绘制前为 0。
    double lastPaintDurationMs() const override
    {
        return m_last_paint_ms;
    }

    /// @brief 返回控件的建议尺寸。
    /// @return 建议尺寸。
    QSize sizeHint() const override;
    /// @brief 返回控件的最小建议尺寸。
    /// @return 最小建议尺寸。
    QSize minimumSizeHint() const override;

    /// @brief 把当前棋盘渲染成完整图像。
    /// @return 可直接导出的图像；没有棋盘时返回空图像。
    /// @note 导出分辨率与当前缩放无关。
    QImage renderBoardImage() const override;

    /// @brief 返回当前每格像素数。
    /// @return 自动适配或手动缩放后的像素数。
    double cellPixels() const;

public slots:
    /// @brief 设置悬停图案的预览位置。
    /// @param offsets 相对悬停格子的图案偏移。
    void setStampPreview(const QVector<QPoint>& offsets);

    /// @brief 查询网格线是否显示。
    /// @return 显示时返回 true。
    bool gridVisible() const;

    /// @brief 设置是否显示网格线。
    /// @param visible 是否显示。
    void setGridVisible(bool visible);

    /// @brief 放大一级。
    void zoomIn();

    /// @brief 缩小一级。
    void zoomOut();

    /// @brief 恢复自动适配缩放并居中。
    void resetZoom();

signals:
    /// @brief 鼠标左键在某个格子上按下。
    /// @param row 格子行下标。
    /// @param col 格子列下标。
    void cellPressed(int row, int col);

    /// @brief 按住左键拖过某个格子。
    /// @param row 格子行下标。
    /// @param col 格子列下标。
    void cellDragged(int row, int col);

    /// @brief 鼠标左键在某个格子上释放。
    /// @param row 格子行下标。
    /// @param col 格子列下标。
    void cellReleased(int row, int col);

protected:
    /// @brief 绘制背景、棋盘、网格与提示文字。
    /// @param event 绘制事件。
    void paintEvent(QPaintEvent* event) override;
    /// @brief 窗口尺寸变化时重新适配缩放或限制平移。
    /// @param event 尺寸事件。
    void resizeEvent(QResizeEvent* event) override;
    /// @brief 处理左键按下与中键/右键开始平移。
    /// @param event 鼠标事件。
    void mousePressEvent(QMouseEvent* event) override;
    /// @brief 处理悬停高亮、拖动绘制与平移。
    /// @param event 鼠标事件。
    void mouseMoveEvent(QMouseEvent* event) override;
    /// @brief 结束落子或平移。
    /// @param event 鼠标事件。
    void mouseReleaseEvent(QMouseEvent* event) override;
    /// @brief 以指针位置为锚点缩放棋盘。
    /// @param event 滚轮事件。
    void wheelEvent(QWheelEvent* event) override;
    /// @brief 鼠标离开控件时清除悬停高亮。
    /// @param event 离开事件。
    void leaveEvent(QEvent* event) override;

private:
    /// @brief 返回棋盘宽度。
    /// @return 列数；没有棋盘时为 0。
    int boardWidth() const;
    /// @brief 返回棋盘高度。
    /// @return 行数；没有棋盘时为 0。
    int boardHeight() const;

    /// @brief 返回棋盘左上角在控件中的位置。
    /// @return 控件坐标。
    QPointF boardTopLeft() const;
    /// @brief 按当前控件尺寸计算适配后的每格像素数。
    /// @return 每格像素数。
    double fitCellPixels() const;
    /// @brief 把控件坐标换算成棋盘格子。
    /// @param position 控件坐标。
    /// @param row 输出行下标。
    /// @param col 输出列下标。
    /// @return 坐标落在棋盘内时返回 true。
    bool cellFromPosition(const QPoint& position, int* row, int* col) const;
    /// @brief 计算棋盘与控件相交区域内的可见格子范围。
    /// @param boardRect 棋盘矩形。
    /// @param cellPixels 每格像素数。
    /// @return 可见范围；完全不可见时为空。
    std::optional<VisibleCellRange> visibleCellRange(const QRectF& boardRect,
                                                     double cellPixels) const;
    /// @brief 把平移量限制在棋盘边缘附近。
    void clampPan();
    /// @brief 以控件中心为锚点缩放。
    /// @param factor 缩放倍数。
    void zoomBy(double factor);
    /// @brief 以指定锚点缩放并保持锚点下的格子不动。
    /// @param anchor 控件坐标中的锚点。
    /// @param factor 缩放倍数。
    void zoomAt(const QPointF& anchor, double factor);

    /// @brief 绘制整个棋盘及其装饰。
    /// @param painter 绘制器。
    /// @param board 要绘制的棋盘。
    /// @param topLeft 棋盘左上角。
    /// @param cellPixels 每格像素数。
    /// @param interactive 是否包含悬停与预览等交互元素。
    void drawBoard(QPainter& painter,
                   const domain::Grid& board,
                   const QPointF& topLeft,
                   double cellPixels,
                   bool interactive) const;
    /// @brief 逐格绘制细胞与悬停高亮。
    /// @param painter 绘制器。
    /// @param board 要绘制的棋盘。
    /// @param topLeft 棋盘左上角。
    /// @param cellPixels 每格像素数。
    /// @param gap 细胞之间的间隙。
    /// @param radius 圆角半径。
    /// @param interactive 是否绘制悬停高亮。
    /// @param range 可见格子范围。
    void drawCellsDetailed(QPainter& painter,
                           const domain::Grid& board,
                           const QPointF& topLeft,
                           double cellPixels,
                           double gap,
                           double radius,
                           bool interactive,
                           const VisibleCellRange& range) const;
    /// @brief 把细胞画进复用图像后再整体贴图。
    /// @param painter 绘制器。
    /// @param board 要绘制的棋盘。
    /// @param boardRect 棋盘矩形。
    /// @param interactive 是否绘制悬停高亮。
    /// @param range 可见格子范围。
    void drawCellsAsImage(QPainter& painter,
                          const domain::Grid& board,
                          const QRectF& boardRect,
                          bool interactive,
                          const VisibleCellRange& range) const;
    /// @brief 重建并缓存网格线位图。
    /// @param boardRect 棋盘矩形。
    /// @param cellPixels 每格像素数。
    void updateGridPixmap(const QRectF& boardRect, double cellPixels) const;
    /// @brief 绘制整个棋盘的网格线。
    /// @param painter 绘制器。
    /// @param boardRect 棋盘矩形。
    /// @param cellPixels 每格像素数。
    /// @param stride 每隔多少格画一条线。
    void drawGridLines(QPainter& painter, const QRectF& boardRect, double cellPixels, int stride = 1) const;
    /// @brief 在指定可见范围内绘制网格线。
    /// @param painter 绘制器。
    /// @param boardRect 棋盘矩形。
    /// @param cellPixels 每格像素数。
    /// @param stride 每隔多少格画一条线。
    /// @param range 可见格子范围。
    void drawGridLinesRange(QPainter& painter,
                            const QRectF& boardRect,
                            double cellPixels,
                            int stride,
                            const VisibleCellRange& range) const;

    const domain::Grid* m_board = nullptr; ///< 当前棋盘；由模型持有，视图不复制。
    int m_population = 0; ///< 当前帧的存活细胞数。

    QVector<QPoint> m_preview; ///< 悬停图案的相对偏移。
    bool m_grid_visible = true; ///< 是否绘制网格线。
    bool m_animating = false; ///< 模拟是否运行；决定是否走图像快速绘制路径。

    mutable QImage m_cell_image; ///< 复用的细胞图像缓存。
    mutable QPixmap m_grid_pixmap; ///< 网格线位图缓存。
    mutable QRectF m_grid_key; ///< 网格缓存对应的棋盘矩形。
    mutable double m_grid_cell = 0.0; ///< 网格缓存对应的每格像素数。
    mutable qreal m_grid_dpr = 0.0; ///< 网格缓存对应的设备像素比。

    double m_cell_pixels = 8.0; ///< 手动缩放时的每格像素数。
    bool m_auto_fit = true; ///< 是否按窗口自动适配缩放。
    QPointF m_pan; ///< 用户平移量。

    double m_last_paint_ms = 0.0; ///< 上一帧绘制耗时（毫秒）。
    int m_hover_row = -1; ///< 鼠标悬停的行；-1 表示没有。
    int m_hover_col = -1; ///< 鼠标悬停的列；-1 表示没有。
    bool m_left_pressed = false; ///< 左键是否按下。
    bool m_panning = false; ///< 是否正在拖动平移。
    QPoint m_last_mouse; ///< 上一次鼠标位置。
};

} // namespace lifegame::view
