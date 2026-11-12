#pragma once

/**
 * @file board_controller.h
 * @brief 鼠标、图案与棋盘命令的输入控制器。
 */

#include <QObject>
#include <QString>
#include <QVector>

#include "domain/pattern.h"

class QAction;
class QPoint;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::view
{
class BoardView;
class ConfirmSink;
class ControlPanel;
class MessageSink;
class PatternCatalog;
} // namespace view

namespace lifegame::controller
{

/**
 * @brief 棋盘输入与命令：绘制、放置图案、清空/随机/示例、网格与缩放。
 *
 * 承担 View→Model 方向：指针手势、图案选择、破坏性命令和缩放。Model→View
 * 的刷新由 BoardPresenter 负责，两者不共享状态。
 */
class BoardController : public QObject
{
    Q_OBJECT

public:
    /// @brief 连接棋盘视图与侧边面板，并推入默认设置。
    ///
    /// 用内置图案填满面板的图案列表、选中画笔，并把默认密度、尺寸粒度与
    /// 网格可见性同时推给视图和面板。
    /// @param model 可写模型。
    /// @param view 接收鼠标输入的棋盘视图。
    /// @param panel 发出意图的侧边面板。
    /// @param catalog 图案展示目录。
    /// @param messages 状态栏输出端口。
    /// @param confirm 破坏性操作的确认端口。
    /// @param parent QObject 父对象。
    BoardController(model::LifeModel* model,
                    view::BoardView* view,
                    view::ControlPanel* panel,
                    const view::PatternCatalog* catalog,
                    view::MessageSink* messages,
                    view::ConfirmSink* confirm,
                    QObject* parent = nullptr);

    /// @brief 关联镜像网格可见性的可勾选菜单动作。
    /// @param action 可勾选的菜单动作。
    void setGridAction(QAction* action);

    /// @brief 查询网格可见性。
    /// @return 网格线可见时返回 true。
    bool gridVisible() const;

    /// @brief 查询随机填充密度。
    /// @return 密度百分比。
    int randomDensity() const;

    /// @brief 修改随机填充密度，并同步到面板。
    /// @param density 密度百分比；按设置范围钳制。
    void setRandomDensity(int density);

    /// @brief 查询棋盘尺寸步进粒度。
    /// @return 步进粒度。
    int sizeStep() const;

    /// @brief 修改棋盘尺寸步进粒度，并同步到面板。
    /// @param step 步进粒度；按设置范围钳制。
    void setSizeStep(int step);

public slots:
    /// @brief 同步设置视图、面板与菜单动作的网格可见性。
    /// @param visible 新的网格可见性。
    void setGridVisible(bool visible);
    /// @brief 确认后清空棋盘。
    void clearBoard();
    /// @brief 按当前密度随机填充棋盘。
    void randomizeBoard();
    /// @brief 载入内置示例图案。
    void loadDemo();
    /// @brief 放大被动棋盘视图。
    void zoomIn();
    /// @brief 缩小被动棋盘视图。
    void zoomOut();
    /// @brief 复位缩放与平移。
    void resetZoom();
    /// @brief 选中一个图案用于点击放置；空 ID 回到画笔。
    /// @param patternId 稳定的图案 ID；为空时选择画笔。
    void selectPattern(const QString& patternId);
    /// @brief 确认后调整棋盘尺寸。
    ///
    /// 调整会清空棋盘，因此非空棋盘需要用户确认；越界数值由模型钳制。
    /// @param width 新宽度。
    /// @param height 新高度。
    void resizeBoard(int width, int height);

private slots:
    /// @brief 应用面板密度并按该密度随机填充。
    /// @param density 密度百分比。
    void onPanelRandomize(int density);
    /// @brief 处理单格按下：开始一次绘制或放置图案。
    /// @param row 按下的行号。
    /// @param col 按下的列号。
    void onCellPressed(int row, int col);
    /// @brief 把当前绘制笔画延伸到拖到的格子。
    /// @param row 拖到的行号。
    /// @param col 拖到的列号。
    void onCellDragged(int row, int col);
    /// @brief 结束当前绘制笔画。
    /// @param row 抬起的行号。
    /// @param col 抬起的列号。
    void onCellReleased(int row, int col);

private:
    /// @brief 绘制两个格子之间栅格化直线上的所有格子。
    /// @param fromRow 起点行号。
    /// @param fromCol 起点列号。
    /// @param toRow 终点行号。
    /// @param toCol 终点列号。
    /// @param alive 写入每个格子的取值。
    void paintLine(int fromRow, int fromCol, int toRow, int toCol, bool alive);
    /// @brief 把选中的图案以中心对齐放到一个格子上。
    /// @param row 中心行号。
    /// @param col 中心列号。
    void applyStamp(int row, int col);

    model::LifeModel* m_model = nullptr;             ///< 可写模型。
    view::BoardView* m_view = nullptr;               ///< 接收鼠标输入的棋盘视图。
    view::ControlPanel* m_panel = nullptr;           ///< 发出意图的侧边面板。
    const view::PatternCatalog* m_catalog = nullptr; ///< 图案展示目录。
    view::MessageSink* m_messages = nullptr;         ///< 状态栏输出端口。
    view::ConfirmSink* m_confirm = nullptr;          ///< 破坏性操作确认端口。

    bool m_painting = false;   ///< 是否有按下并拖动的绘制笔画在进行中。
    bool m_paintValue = false; ///< 当前笔画写入的取值。
    int m_lastRow = -1;        ///< 上一格行号；-1 表示笔画尚无锚点。
    int m_lastCol = -1;        ///< 上一格列号；-1 表示笔画尚无锚点。

    bool m_stampMode = false; ///< 是否选中图案（否则为画笔）。
    domain::Pattern m_stamp;  ///< 图案模式下点击放置的图案。

    bool m_gridVisible = true;       ///< 网格可见性，同时镜像到菜单动作。
    int m_density = 30;              ///< 随机填充密度百分比。
    int m_sizeStep = 5;              ///< 棋盘尺寸步进粒度。
    QAction* m_gridAction = nullptr; ///< 镜像网格可见性的可勾选菜单动作。
};

} // namespace lifegame::controller
