#include "service/rle_file_service.h"

#include <stacklog/stacklog.hpp>

#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QIODevice>

#include <string_view>

namespace lifegame::service
{

namespace
{

constexpr qint64 kMaxRleFileBytes = 8LL * 1024 * 1024;

} // namespace

RleReadResult RleFileService::read(const QString& path) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        STACKLOG_WARN() << "RLE open failed path=" << path.toStdString() << " error="
                        << file.errorString().toStdString();
        return {std::nullopt, FileError::OpenFailed, {}, file.errorString()};
    }
    if (file.size() > kMaxRleFileBytes)
    {
        STACKLOG_WARN() << "RLE read rejected reason=too_large path=" << path.toStdString()
                        << " bytes=" << file.size();
        return {std::nullopt, FileError::TooLarge, {}, {}};
    }

    // Read at most one byte past the cap: file.size() is -1 for pipes and
    // other special files, so readAll() could otherwise consume unbounded data.
    QByteArray data;
    while (data.size() <= static_cast<int>(kMaxRleFileBytes))
    {
        const qint64 remaining = kMaxRleFileBytes + 1 - data.size();
        const QByteArray chunk = file.read(remaining);
        if (chunk.isEmpty())
        {
            break;
        }
        data += chunk;
    }
    if (data.size() > kMaxRleFileBytes)
    {
        STACKLOG_WARN() << "RLE read rejected reason=too_large path=" << path.toStdString() << " bytes="
                        << data.size();
        return {std::nullopt, FileError::TooLarge, {}, {}};
    }
    if (file.error() != QFileDevice::NoError)
    {
        STACKLOG_WARN() << "RLE read failed path=" << path.toStdString() << " error="
                        << file.errorString().toStdString();
        return {std::nullopt, FileError::ReadFailed, {}, file.errorString()};
    }

    const domain::RleDecodeResult decoded =
        domain::decodeRle(std::string_view(data.constData(), static_cast<std::size_t>(data.size())));
    if (!decoded.document.has_value())
    {
        STACKLOG_WARN() << "RLE decode failed path=" << path.toStdString() << " code="
                        << rleErrorName(decoded.error.code) << " line=" << decoded.error.line
                        << " column=" << decoded.error.column;
        return {std::nullopt, FileError::DecodeFailed, decoded.error, {}};
    }
    STACKLOG_DEBUG() << "RLE read path=" << path.toStdString() << " bytes=" << data.size();
    return {decoded.document, FileError::None, {}, {}};
}

FileError RleFileService::write(const QString& path, const QString& text, QString* detail) const
{
    // QSaveFile writes to a temporary file and commits atomically, so a failed
    // write cannot truncate an existing user file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        if (detail != nullptr)
        {
            *detail = file.errorString();
        }
        STACKLOG_WARN() << "RLE write open failed path=" << path.toStdString() << " error="
                        << file.errorString().toStdString();
        return FileError::OpenFailed;
    }

    const QByteArray data = text.toUtf8();
    if (file.write(data) != data.size() || !file.commit())
    {
        if (detail != nullptr)
        {
            *detail = file.errorString();
        }
        STACKLOG_WARN() << "RLE write failed path=" << path.toStdString() << " error="
                        << file.errorString().toStdString();
        return FileError::WriteFailed;
    }
    STACKLOG_DEBUG() << "RLE written path=" << path.toStdString() << " bytes=" << data.size();
    return FileError::None;
}

} // namespace lifegame::service
