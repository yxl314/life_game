#pragma once

/**
 * @file board_controller.h
 * @brief Mouse, pattern and board-command input controller.
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

/// @brief Board input and commands: painting, stamping, clear/random/demo, grid.
class BoardController : public QObject
{
    Q_OBJECT

public:
    /**
     * @param model Writable model.
     * @param view Board view for mouse input.
     * @param panel Side panel for intents.
     * @param catalog Pattern presentation catalog.
     * @param messages Status output port.
     * @param confirm Destructive-action confirmation port.
     * @param parent QObject parent.
     */
    BoardController(model::LifeModel* model,
                    view::BoardView* view,
                    view::ControlPanel* panel,
                    const view::PatternCatalog* catalog,
                    view::MessageSink* messages,
                    view::ConfirmSink* confirm,
                    QObject* parent = nullptr);

    /// @param action Checkable menu action mirroring grid visibility.
    void setGridAction(QAction* action);

    /// @return Current grid visibility.
    bool gridVisible() const;

    /// @return Current random-fill density percentage.
    int randomDensity() const;

    /// @param density Random-fill density percentage; clamped to the settings range.
    void setRandomDensity(int density);

    /// @return Board-size spinner step.
    int sizeStep() const;

    /// @param step Board-size spinner step; clamped to the settings range.
    void setSizeStep(int step);

public slots:
    /// @param visible New grid visibility.
    void setGridVisible(bool visible);
    /// @brief Clear the board after confirmation.
    void clearBoard();
    /// @brief Randomize the board.
    void randomizeBoard();
    /// @brief Load the built-in demo pattern.
    void loadDemo();
    /// @brief Zoom the passive board view in.
    void zoomIn();
    /// @brief Zoom the passive board view out.
    void zoomOut();
    /// @brief Reset board zoom and pan.
    void resetZoom();
    /// @param patternId Stable pattern ID; empty selects the brush.
    void selectPattern(const QString& patternId);
    /// @param width New width.
    /// @param height New height.
    void resizeBoard(int width, int height);

private slots:
    void onPanelRandomize(int density);
    void onCellPressed(int row, int col);
    void onCellDragged(int row, int col);
    void onCellReleased(int row, int col);

private:
    void paintLine(int fromRow, int fromCol, int toRow, int toCol, bool alive);
    void applyStamp(int row, int col);

    model::LifeModel* m_model = nullptr;
    view::BoardView* m_view = nullptr;
    view::ControlPanel* m_panel = nullptr;
    const view::PatternCatalog* m_catalog = nullptr;
    view::MessageSink* m_messages = nullptr;
    view::ConfirmSink* m_confirm = nullptr;

    bool m_painting = false;
    bool m_paintValue = false;
    int m_lastRow = -1;
    int m_lastCol = -1;

    bool m_stampMode = false;
    domain::Pattern m_stamp;

    bool m_gridVisible = true;
    int m_density = 30;
    int m_sizeStep = 5;
    QAction* m_gridAction = nullptr;
};

} // namespace lifegame::controller
