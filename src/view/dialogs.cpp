#include "view/dialogs.h"

#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>
#include <QPushButton>

namespace lifegame::view
{

void showWarning(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton, parent);
    QPushButton* close = box.addButton(QCoreApplication::translate("Dialogs", "确定"),
                                       QMessageBox::AcceptRole);
    close->setIcon(QIcon(QStringLiteral(":/icons/check.svg")));
    box.exec();
}

void showControlsHelp(QWidget* parent)
{
    QMessageBox box(parent);
    box.setWindowTitle(QCoreApplication::translate("Dialogs", "操作说明"));
    box.setTextFormat(Qt::RichText);
    box.setText(QCoreApplication::translate(
        "Dialogs",
        "<h3>操作说明</h3>"
        "<table cellspacing='6'>"
        "<tr><td><b>左键</b></td><td>切换细胞；按住拖动可连续绘制或擦除</td></tr>"
        "<tr><td><b>中键 / 右键拖动</b></td><td>平移棋盘</td></tr>"
        "<tr><td><b>滚轮</b></td><td>以鼠标位置为中心缩放</td></tr>"
        "<tr><td><b>空格</b></td><td>开始 / 暂停</td></tr>"
        "<tr><td><b>F6</b></td><td>单步演化一代</td></tr>"
        "<tr><td><b>Ctrl+Shift+← / →</b></td><td>按面板代数回退 / 快进</td></tr>"
        "<tr><td><b>Ctrl+R</b></td><td>随机填充</td></tr>"
        "<tr><td><b>Ctrl+D</b></td><td>载入示例图案</td></tr>"
        "<tr><td><b>Ctrl+0</b></td><td>重置缩放</td></tr>"
        "<tr><td><b>Ctrl+,</b></td><td>打开设置</td></tr>"
        "</table>"
        "<p>在侧边「图案」下拉框中选择图案后，鼠标会显示半透明预览，"
        "点击棋盘即可放置。选择「画笔」可回到直接绘制模式。</p>"));
    box.addButton(QCoreApplication::translate("Dialogs", "关闭"), QMessageBox::AcceptRole);
    box.exec();
}

void showAbout(QWidget* parent, const QString& version)
{
    QMessageBox box(parent);
    box.setWindowTitle(QCoreApplication::translate("Dialogs", "关于 生命游戏"));
    box.setTextFormat(Qt::RichText);
    box.setText(QCoreApplication::translate(
                    "Dialogs",
                    "<h3>生命游戏</h3>"
                    "<p>Conway's Game of Life，1970 年由 John Horton Conway 提出。</p>"
                    "<p>每个细胞的状态只由邻居数量决定：邻居恰为 3 个时诞生，"
                    "邻居为 2 或 3 个时存活，其余情况死亡。看似简单的规则可以产生"
                    "滑翔机、振荡器、甚至能自我复制的图案。</p>"
                    "<p>版本 %1<br>"
                    "技术：C++17 · Qt 6 · 经典分层 MVC<br>"
                    "Domain 层为纯 C++，可脱离 Qt 单独测试。</p>")
                    .arg(version));
    box.addButton(QCoreApplication::translate("Dialogs", "确定"), QMessageBox::AcceptRole);
    box.exec();
}

void showAboutQt(QWidget* parent)
{
    QMessageBox box(parent);
    box.setWindowTitle(QCoreApplication::translate("Dialogs", "关于 Qt"));
    box.setText(QCoreApplication::translate("Dialogs", "Qt 版本：%1")
                    .arg(QString::fromLatin1(qVersion())));
    box.addButton(QCoreApplication::translate("Dialogs", "确定"), QMessageBox::AcceptRole);
    box.exec();
}

} // namespace lifegame::view
