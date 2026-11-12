#pragma once

/**
 * @file rle_file_service.h
 * @brief RLE file I/O port and service.
 */

#include <QString>

#include <optional>

#include "domain/rle.h"
#include "service/file_error.h"

namespace lifegame::service
{

/// @brief Result of reading and decoding an RLE file.
struct RleReadResult
{
    std::optional<domain::DecodedDocument> document; ///< Decoded document on success.
    FileError error = FileError::None;               ///< Typed file error.
    domain::RleErrorInfo decodeError;                ///< RLE parser details on decode failure.
    QString detail;                                  ///< Optional system detail.
};

/// @brief Replaceable RLE file I/O port.
class RleFilePort
{
public:
    /// @brief Allow deletion through the port interface.
    virtual ~RleFilePort() = default;

    /**
     * @brief Read a file and decode it as RLE.
     *
     * @param path File path.
     * @return Decoded document or a typed file/decode error.
     */
    virtual RleReadResult read(const QString& path) const = 0;

    /**
     * @brief Atomically write RLE text.
     * @param path Destination path.
     * @param text RLE text.
     * @param detail Optional system error detail.
     * @return FileError code.
     */
    virtual FileError write(const QString& path, const QString& text, QString* detail = nullptr) const = 0;
};

/// @brief File I/O only; RLE parsing is delegated to the pure domain codec.
class RleFileService : public RleFilePort
{
public:
    /**
     * @brief Read at most 8 MiB, then decode the bytes as RLE.
     *
     * The size cap is enforced while reading rather than from file.size(),
     * which is -1 for pipes and other special files.
     *
     * @param path File path.
     * @return Decoded document or a typed file/decode error.
     * @retval OpenFailed The file cannot be opened for reading.
     * @retval TooLarge The file exceeds 8 MiB.
     * @retval ReadFailed The read itself failed.
     * @retval DecodeFailed The bytes are not a valid RLE document.
     */
    RleReadResult read(const QString& path) const override;

    /**
     * @brief Write UTF-8 text through QSaveFile, so a failed write keeps the old file.
     *
     * @param path Destination path.
     * @param text RLE text.
     * @param detail Optional system error detail.
     * @return FileError code.
     * @retval OpenFailed The temporary file cannot be created.
     * @retval WriteFailed Writing or committing the temporary file failed.
     */
    FileError write(const QString& path, const QString& text, QString* detail = nullptr) const override;
};

} // namespace lifegame::service
