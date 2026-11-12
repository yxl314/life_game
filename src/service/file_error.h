#pragma once

/**
 * @file file_error.h
 * @brief 类型化的文件与图像错误。
 */

namespace lifegame::service
{

/// @brief 可恢复的基础设施失败；界面把它们转成一条提示。
enum class FileError
{
    None = 0,      ///< 成功。
    OpenFailed,    ///< 无法打开文件。
    ReadFailed,    ///< 读取过程中的错误。
    WriteFailed,   ///< 写入或刷新失败。
    DecodeFailed,  ///< 内容不是合法的 RLE。
    EncodeFailed,  ///< PNG 编码器失败。
    NullImage,     ///< 传给导出器的是空图像。
    TooLarge       ///< 文件超过配置的大小上限。
};

/// @brief 错误码的固定拼写，用于日志。
/// @param error 错误码。
/// @return 静态名称；枚举外的取值返回 "Unknown"。
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
