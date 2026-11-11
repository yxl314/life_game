#include "view/theme.h"

#include <stacklog/stacklog.h>

#include <QApplication>
#include <QFile>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyleFactory>

static void initResources()
{
    Q_INIT_RESOURCE(resources);
}

namespace lifegame::view::theme
{

namespace
{

void applyDarkPalette(QApplication& app)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(13, 17, 23));
    palette.setColor(QPalette::WindowText, QColor(201, 209, 217));
    palette.setColor(QPalette::Base, QColor(10, 13, 18));
    palette.setColor(QPalette::AlternateBase, QColor(22, 27, 34));
    palette.setColor(QPalette::ToolTipBase, QColor(22, 27, 34));
    palette.setColor(QPalette::ToolTipText, QColor(201, 209, 217));
    palette.setColor(QPalette::Text, QColor(201, 209, 217));
    palette.setColor(QPalette::Button, QColor(33, 38, 45));
    palette.setColor(QPalette::ButtonText, QColor(201, 209, 217));
    palette.setColor(QPalette::BrightText, QColor(255, 255, 255));
    palette.setColor(QPalette::Link, QColor(88, 166, 255));
    palette.setColor(QPalette::Highlight, QColor(35, 134, 54));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(110, 118, 129));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(110, 118, 129));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(110, 118, 129));
    app.setPalette(palette);
}

void applyStyleSheet(QApplication& app)
{
    QFile file(QStringLiteral(":/style.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        STACKLOG_WARN() << "style sheet resource missing: :/style.qss";
        return;
    }
    app.setStyleSheet(QString::fromUtf8(file.readAll()));
}

} // namespace

void apply(QApplication& app)
{
    initResources();

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    applyDarkPalette(app);
    applyStyleSheet(app);
    app.setWindowIcon(appIcon());
}

QIcon appIcon()
{
    const int size = 256;
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF plate(10, 10, size - 20, size - 20);
    QLinearGradient gradient(0, 0, 0, size);
    gradient.setColorAt(0.0, QColor(30, 38, 48));
    gradient.setColorAt(1.0, QColor(12, 16, 22));
    painter.setBrush(gradient);
    painter.setPen(QPen(QColor(63, 185, 80, 180), 5));
    painter.drawRoundedRect(plate, 54, 54);

    painter.setPen(QPen(QColor(255, 255, 255, 12), 1));
    for (int i = 1; i < 6; ++i)
    {
        const qreal offset = 42.0 + i * 34.0;
        painter.drawLine(QPointF(offset, 42), QPointF(offset, 214));
        painter.drawLine(QPointF(42, offset), QPointF(214, offset));
    }

    const int cell = 44;
    const int gap = 4;
    const int originX = 62;
    const int originY = 62;
    const int glider[3][3] = {{0, 1, 0}, {0, 0, 1}, {1, 1, 1}};

    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            if (glider[row][col] == 0)
            {
                continue;
            }
            const QRectF rect(originX + col * cell + gap / 2.0,
                              originY + row * cell + gap / 2.0,
                              cell - gap,
                              cell - gap);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(63, 185, 80, 55));
            painter.drawRoundedRect(rect.adjusted(-5, -5, 5, 5), 12, 12);
            painter.setBrush(QColor(84, 214, 100));
            painter.drawRoundedRect(rect, 8, 8);
        }
    }

    painter.end();
    return QIcon(pixmap);
}

} // namespace lifegame::view::theme
