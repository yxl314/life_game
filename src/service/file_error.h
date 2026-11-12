#pragma once

/**
 * @file file_error.h
 * @brief Typed file and image errors.
 */

namespace lifegame::service
{

/// @brief Recoverable infrastructure failures. The UI turns these into one message.
enum class FileError
{
    None = 0,      ///< Success.
    OpenFailed,    ///< Could not open the file.
    ReadFailed,    ///< Read/error while loading.
    WriteFailed,   ///< Write/flush failed.
    DecodeFailed,  ///< Content is not valid RLE.
    EncodeFailed,  ///< PNG encoder failed.
    NullImage,     ///< Null image passed to the exporter.
    TooLarge       ///< File exceeds the configured size cap.
};

/// @brief Stable spelling of an error code for logs.
///
/// @param error Error code.
/// @return Static name; "Unknown" for a value outside the enumeration.
constexpr const char* fileErrorName(FileError error) noexcept
{
    switch (error)
    {
    case FileError::None:
        return "None";
    case FileError::OpenFailed:
        return "OpenFailed";
    case FileError::ReadFailed:
        return "ReadFailed";
    case FileError::WriteFailed:
        return "WriteFailed";
    case FileError::DecodeFailed:
        return "DecodeFailed";
    case FileError::EncodeFailed:
        return "EncodeFailed";
    case FileError::NullImage:
        return "NullImage";
    case FileError::TooLarge:
        return "TooLarge";
    }
    return "Unknown";
}

} // namespace lifegame::service
