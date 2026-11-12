#pragma once

/**
 * @file session_service.h
 * @brief 与 UI 无关的会话持久化服务。
 */

#include <QByteArray>
#include <QString>

#include "model/command_result.h"
#include "service/settings_error.h"
#include "shared/app_settings.h"

namespace lifegame::domain
{
struct RleErrorInfo;
}

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class SettingsStore;
}

namespace lifegame::controller
{

/// @brief 恢复持久化会话数据的结果分类。
enum class SessionRestoreStatus
{
    NoStoredBoard, ///< 没有持久化棋盘；设置仍可能已载入。
    Restored,      ///< 棋盘与代数都已恢复。
    PartialRestore,///< 棋盘已恢复，但代数未能应用。
    DecodeFailed,  ///< 保存的 RLE 格式错误。
    LoadFailed     ///< 模型拒绝了文档，或回退分配失败。
};

/// @brief SessionService::restore() 的结果。
struct SessionRestoreResult
{
    shared::AppSettings settings;              ///< 归一化后的设置。
    service::SettingsError error = service::SettingsError::None; ///< 设置错误。
    QString detail;                            ///< 可选的错误细节。
    SessionRestoreStatus status = SessionRestoreStatus::NoStoredBoard; ///< 恢复结果分类。
    model::CommandResult modelResult = model::CommandResult::Success;  ///< 模型失败细节。
    QByteArray windowGeometry;                 ///< 已保存的窗口几何。

    /// @brief 判断持久化数据里是否带有棋盘。
    /// @return 带棋盘载荷时返回 true。
    bool hadStoredBoard() const noexcept
    {
        return status != SessionRestoreStatus::NoStoredBoard;
    }

    /// @brief 判断棋盘本身是否已恢复。
    /// @return 至少棋盘已恢复时返回 true。
    bool restored() const noexcept
    {
        return status == SessionRestoreStatus::Restored ||
               status == SessionRestoreStatus::PartialRestore;
    }
};

/**
 * @brief 与 UI 无关的会话持久化。
 *
 * 把恢复出的设置、规则与边界应用到模型，解码保存的 RLE，并在没有任何
 * 可用数据时回退到默认棋盘尺寸。所有提示都通过返回值交给调用方。
 */
class SessionService
{
public:
    /// @brief 绑定模型与持久化存储。
    /// @param model 需要恢复/保存的模型。
    /// @param store 持久化存储。
    SessionService(model::LifeModel* model, service::SettingsStore* store);

    /// @brief 读取已保存状态并写入模型。
    /// @return 恢复结果与回退后的棋盘状态。
    SessionRestoreResult restore();

    /// @brief 持久化当前模型状态与设置。
    /// @param settings 要保存的设置。
    /// @param windowGeometry 要保存的窗口几何。
    /// @param detail 可选的错误细节。
    /// @return 设置错误码。
    service::SettingsError save(const shared::AppSettings& settings,
                                const QByteArray& windowGeometry,
                                QString* detail = nullptr);

private:
    model::LifeModel* m_model = nullptr;       ///< 需要恢复/保存的模型。
    service::SettingsStore* m_store = nullptr; ///< 持久化存储。
};

} // namespace lifegame::controller
