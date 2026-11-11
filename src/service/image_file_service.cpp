#include "service/image_file_service.h"

#include <stacklog/stacklog.h>

#include <QImage>
#include <QImageWriter>

namespace lifegame::service
{

FileError ImageFileService::savePng(const QImage& image, const QString& path, QString* detail) const
{
    if (image.isNull())
    {
        return FileError::NullImage;
    }

    QImageWriter writer(path, "png");
    if (!writer.write(image))
    {
        if (detail != nullptr)
        {
            *detail = writer.errorString();
        }
        STACKLOG_WARN() << "PNG write failed path=" << path.toStdString() << " error="
                        << writer.errorString().toStdString();
        return FileError::EncodeFailed;
    }
    STACKLOG_INFO() << "PNG written path=" << path.toStdString() << " size=" << image.width() << 'x'
                    << image.height();
    return FileError::None;
}

} // namespace lifegame::service
