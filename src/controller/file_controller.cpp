#include "controller/file_controller.h"

#include "controller/controller_log.h"

#include "view/dialogs.h"

#include "controller/error_text.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QImage>

#include <string>

#include "domain/rle.h"
#include "service/image_file_service.h"
#include "service/rle_file_service.h"
#include "model/life_model.h"
#include "view/interfaces.h"

namespace lifegame::controller
{

FileController::FileController(model::LifeModel* model,
                               view::BoardImageSource* imageSource,
                               view::MessageSink* messages,
                               QWidget* dialogParent,
                               service::RleFilePort* rleService,
                               service::ImageFilePort* imageService,
                               QObject* parent)
    : QObject(parent),
      m_model(model),
      m_imageSource(imageSource),
      m_messages(messages),
      m_dialogParent(dialogParent),
      m_rle(rleService),
      m_image(imageService),
      m_lastDir(QDir::homePath())
{
}

QString FileController::rleErrorMessage(const domain::RleErrorInfo& error) const
{
    switch (error.code)
    {
    case domain::RleError::None:
        return {};
    case domain::RleError::EmptyBody:
        return tr("文件里没有 RLE 正文。");
    case domain::RleError::InvalidHeader:
        return tr("RLE 头部无法解析。");
    case domain::RleError::InvalidCharacter:
        return tr("无法识别的 RLE 字符（第 %1 行第 %2 列）。").arg(error.line).arg(error.column);
    case domain::RleError::RunTooLong:
        return tr("RLE 游程长度异常。");
    case domain::RleError::TooLarge:
        return tr("图案尺寸超出棋盘上限（%1 × %2）。")
            .arg(domain::kMaxBoardWidth)
            .arg(domain::kMaxBoardHeight);
    case domain::RleError::InvalidRule:
        return tr("RLE 头部规则记号无效。");
    case domain::RleError::MissingTerminator:
        return tr("RLE 数据缺少结束标记“!”，文件可能被截断。");
    }
    return {};
}

FileController::OpenResult FileController::openPath(const QString& path)
{
    OpenResult output;
    const service::RleReadResult result = m_rle->read(path);
    output.fileError = result.error;
    output.decodeError = result.decodeError;
    output.detail = result.detail;
    if (result.error != service::FileError::None || !result.document.has_value())
    {
        return output;
    }

    output.modelResult = m_model->loadDocument(*result.document);
    return output;
}

void FileController::openRle()
{
    const QString path = QFileDialog::getOpenFileName(
        m_dialogParent,
        tr("打开 RLE 图案"),
        m_lastDir,
        tr("生命游戏图案 (*.rle *.RLE *.txt);;所有文件 (*)"));
    if (path.isEmpty())
    {
        qCDebug(lcController) << "open RLE cancelled";
        return;
    }

    const OpenResult result = openPath(path);
    if (result.fileError == service::FileError::DecodeFailed)
    {
        view::showWarning(m_dialogParent, tr("打开失败"), rleErrorMessage(result.decodeError));
        return;
    }
    if (result.fileError != service::FileError::None)
    {
        view::showWarning(m_dialogParent, tr("打开失败"), fileErrorText(result.fileError, result.detail));
        return;
    }
    if (model::failed(result.modelResult))
    {
        view::showWarning(m_dialogParent, tr("打开失败"), commandResultText(result.modelResult));
        return;
    }

    m_lastDir = QFileInfo(path).absolutePath();
    qCInfo(lcController) << "RLE opened path=" << path << "board=" << m_model->width() << 'x'
                         << m_model->height() << "population" << m_model->stats().population;
    if (m_messages != nullptr)
    {
        m_messages->showMessage(tr("已打开 %1（%2 × %3）。")
                                    .arg(QFileInfo(path).fileName())
                                    .arg(m_model->width())
                                    .arg(m_model->height()),
                                6000);
    }
}

service::FileError FileController::savePath(const QString& path, QString* detail)
{
    const QString text = QString::fromStdString(m_model->toRle("life-game"));
    const service::FileError error = m_rle->write(path, text, detail);
    if (error == service::FileError::None)
    {
        qCInfo(lcController) << "RLE saved path=" << path << "bytes=" << text.size();
    }
    return error;
}

void FileController::saveRle()
{
    QString path = QFileDialog::getSaveFileName(
        m_dialogParent,
        tr("保存为 RLE"),
        m_lastDir + QStringLiteral("/life-game.rle"),
        tr("生命游戏图案 (*.rle);;所有文件 (*)"));
    if (path.isEmpty())
    {
        qCDebug(lcController) << "save RLE cancelled";
        return;
    }
    if (QFileInfo(path).suffix().isEmpty())
    {
        path += QStringLiteral(".rle");
    }

    QString detail;
    const service::FileError error = savePath(path, &detail);
    if (error != service::FileError::None)
    {
        view::showWarning(m_dialogParent, tr("保存失败"), fileErrorText(error, detail));
        return;
    }

    m_lastDir = QFileInfo(path).absolutePath();
    if (m_messages != nullptr)
    {
        m_messages->showMessage(tr("已保存到 %1。").arg(QFileInfo(path).fileName()), 6000);
    }
}

service::FileError FileController::exportPath(const QString& path, QString* detail)
{
    const QImage image = m_imageSource->renderBoardImage();
    const service::FileError error = m_image->savePng(image, path, detail);
    if (error == service::FileError::None)
    {
        qCInfo(lcController) << "PNG exported path=" << path << "size=" << image.width() << 'x'
                             << image.height();
    }
    return error;
}

void FileController::exportImage()
{
    QString path = QFileDialog::getSaveFileName(
        m_dialogParent,
        tr("导出 PNG 图片"),
        m_lastDir + QStringLiteral("/life-game.png"),
        tr("PNG 图片 (*.png);;所有文件 (*)"));
    if (path.isEmpty())
    {
        qCDebug(lcController) << "export PNG cancelled";
        return;
    }
    if (QFileInfo(path).suffix().isEmpty())
    {
        path += QStringLiteral(".png");
    }

    QString detail;
    const service::FileError error = exportPath(path, &detail);
    if (error != service::FileError::None)
    {
        view::showWarning(m_dialogParent, tr("导出失败"), fileErrorText(error, detail));
        return;
    }

    m_lastDir = QFileInfo(path).absolutePath();
    if (m_messages != nullptr)
    {
        m_messages->showMessage(tr("已导出图片 %1。").arg(QFileInfo(path).fileName()), 6000);
    }
}

} // namespace lifegame::controller
