#pragma once

/**
 * @file settings_error.h
 * @brief 类型化的设置持久化错误。
 */

namespace lifegame::service
{

/// @brief 读写会话文件时可恢复的失败类型。
enum class SettingsError
{
    None = 0,      ///< 成功。
    AccessError,   ///< 文件无法读取或写入。
    FormatError,   ///< QSettings 格式非法。
    CorruptData    ///< 一个或多个取值畸形，已被钳制或替换。
};

/// @brief 错误码的固定拼写，用于日志。
/// @param error 错误码。
/// @return 静态名称；枚举外的取值返回 "Unknown"。
constexpr const char* settingsErrorName(SettingsError error) noexcept
{
    switch (error)
    {
    case SettingsError::None:
        return "None";
    case SettingsError::AccessError:
        return "AccessError";
    case SettingsError::FormatError:
        return "FormatError";
    case SettingsError::CorruptData:
        return "CorruptData";
    }
    return "Unknown";
}

} // namespace lifegame::service
