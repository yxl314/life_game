#pragma once

/**
 * @file error_text.h
 * @brief 错误码到本地化提示文案的映射。
 */

#include <QString>

#include "model/command_result.h"
#include "service/file_error.h"
#include "service/settings_error.h"

namespace lifegame::controller
{

/// @brief 把模型命令结果转换为一条可操作的提示。
/// @param result 模型命令结果。
/// @return 本地化文案；model::CommandResult::Success 返回空串。
QString commandResultText(model::CommandResult result);

/// @brief 把文件错误转换为一条可操作的提示。
/// @param error 文件错误。
/// @param detail 可选的系统细节，会追加在括号内。
/// @return 本地化文案；service::FileError::None 返回空串。
QString fileErrorText(service::FileError error, const QString& detail = {});

/// @brief 把设置错误转换为一条可操作的提示。
/// @param error 设置错误。
/// @param detail 可选的系统细节，会追加在括号内。
/// @return 本地化文案；service::SettingsError::None 返回空串。
QString settingsErrorText(service::SettingsError error, const QString& detail = {});

} // namespace lifegame::controller
