#pragma once

/**
 * @file settings_store.h
 * @brief 基于 QSettings 的持久化。
 */

#include <QString>

#include <memory>

#include "service/session_state.h"
#include "service/settings_error.h"

class QSettings;

namespace lifegame::service
{

/// @brief 读取持久化会话数据的结果。
struct SettingsLoadResult
{
    SessionState state;                        ///< 读取并归一化后的状态。
    SettingsError error = SettingsError::None; ///< 读取错误（若有）。
    QString detail;                            ///< 可选的文件/细节文本。
};

/**
 * @brief 基于 QSettings 的持久化；不含界面，也不含模型状态机。
 *
 * 键名只在本类内部出现，调用方只看得到 SessionState。每个标量在读取时
 * 都会做范围检查：类型不可解析的值回落到默认值并置 SettingsError::CorruptData，
 * 可解析但越界的值被钳制到边界、错误保持 None。两种处理都不会让恢复整体失败。
 */
class SettingsStore
{
public:
    /// @brief 接管所有读写使用的 QSettings 实例。
    /// @param settings 拥有所有权的 QSettings 实例（仅测试中可以为空）。
    explicit SettingsStore(std::unique_ptr<QSettings> settings);
    /// @brief 释放持有的 QSettings 实例。
    ~SettingsStore();

    /// @brief 存储独占自己的 QSettings 实例，因此禁止拷贝。
    SettingsStore(const SettingsStore&) = delete;
    /// @brief 存储独占自己的 QSettings 实例，因此禁止赋值。
    SettingsStore& operator=(const SettingsStore&) = delete;

    /// @brief 读取并校验已保存的会话状态。
    /// @return 读取结果、默认值与相应的类型化错误。
    /// @retval AccessError 没有 QSettings 实例，或文件不可读。
    /// @retval FormatError QSettings 不认可该文件格式。
    /// @retval CorruptData 某个取值不可用，已被默认值替换。
    SettingsLoadResult load() const;

    /**
     * @brief 保存会话状态。
     * @param state 待写入的状态。
     * @param detail 可选的文件/细节输出。
     * @return 设置错误码。
     */
    SettingsError save(const SessionState& state, QString* detail = nullptr);

private:
    std::unique_ptr<QSettings> m_settings; ///< QSettings 实例；为空时不做任何读写。
};

} // namespace lifegame::service
