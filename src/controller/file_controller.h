#pragma once

/**
 * @file file_controller.h
 * @brief RLE open/save and PNG export controller.
 */

#include <QObject>
#include <QString>

#include "domain/rle.h"
#include "model/command_result.h"
#include "service/file_error.h"

class QWidget;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class RleFilePort;
class ImageFilePort;
}

namespace lifegame::view
{
class BoardImageSource;
class MessageSink;
}

namespace lifegame::controller
{

/// @brief RLE open/save and PNG export controller.
class FileController : public QObject
{
    Q_OBJECT

public:
    /// @brief Combined file/decode/model result of opening a path.
    struct OpenResult
    {
        service::FileError fileError = service::FileError::None; ///< I/O/decode error.
        domain::RleErrorInfo decodeError;                        ///< RLE parser details.
        model::CommandResult modelResult = model::CommandResult::Success; ///< Model load result.
        QString detail;                                          ///< Optional system detail.
    };

    /// @param model Writable model.
    /// @param imageSource Narrow source for PNG export.
    /// @param messages Status output port.
    /// @param dialogParent Parent for file dialogs.
    /// @param rleService RLE file port.
    /// @param imageService PNG output port.
    /// @param parent QObject parent.
    FileController(model::LifeModel* model,
                   view::BoardImageSource* imageSource,
                   view::MessageSink* messages,
                   QWidget* dialogParent,
                   service::RleFilePort* rleService,
                   service::ImageFilePort* imageService,
                   QObject* parent = nullptr);

    /// @param path RLE path.
    /// @return Combined open result.
    OpenResult openPath(const QString& path);

    /// @param path Destination path.
    /// @param detail Optional error detail.
    /// @return File error code.
    service::FileError savePath(const QString& path, QString* detail = nullptr);

    /// @param path PNG destination.
    /// @param detail Optional error detail.
    /// @return File error code.
    service::FileError exportPath(const QString& path, QString* detail = nullptr);

public slots:
    void openRle();     ///< Show open dialog.
    void saveRle();     ///< Show save dialog.
    void exportImage(); ///< Show PNG export dialog.

private:
    QString rleErrorMessage(const domain::RleErrorInfo& error) const;

    model::LifeModel* m_model = nullptr;
    view::BoardImageSource* m_imageSource = nullptr;
    view::MessageSink* m_messages = nullptr;
    QWidget* m_dialogParent = nullptr;
    service::RleFilePort* m_rle = nullptr;
    service::ImageFilePort* m_image = nullptr;
    QString m_lastDir;
};

} // namespace lifegame::controller
