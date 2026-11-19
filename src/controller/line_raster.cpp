#include "controller/line_raster.h"

#include <cstdlib>

namespace lifegame::controller
{

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
std::vector<CellPosition> rasterLine(int fromRow, int fromCol, int toRow, int toCol)
{
    std::vector<CellPosition> cells;

    int x0 = fromCol;
    int y0 = fromRow;
    const int x1 = toCol;
    const int y1 = toRow;

    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;

    for (;;)
    {
        cells.push_back({y0, x0});
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        const int twice = 2 * error;
        if (twice >= dy)
        {
            error += dy;
            x0 += sx;
        }
        if (twice <= dx)
        {
            error += dx;
            y0 += sy;
        }
    }

    return cells;
}

} // namespace lifegame::controller
