#pragma once

/**
 * @file settings_dialog.h
 * @brief 设置值的编辑视图。
 */

#include <QDialog>

#include "shared/app_settings.h"

class QSpinBox;

namespace lifegame::view
{

/// @brief 纯视图：读写 AppSettings，不负责持久化，也不依赖模型。
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /// @brief 构造设置对话框并创建输入控件。
    /// @param parent 父控件。
    explicit SettingsDialog(QWidget* parent = nullptr);

    /// @brief 显示设置值。
    /// @param values 当前设置；未编辑的字段会保留。
    void setValues(const shared::AppSettings& values);

    /// @brief 返回编辑后的设置。
    /// @return 已编辑字段与保留字段合并后的设置。
    shared::AppSettings values() const;

private:
    QSpinBox* m_jumpStepSpin = nullptr; ///< 跳转粒度输入框。
    QSpinBox* m_sizeStepSpin = nullptr; ///< 尺寸粒度输入框。
    QSpinBox* m_snapshotSpin = nullptr; ///< 快照间隔输入框。
    shared::AppSettings m_base; ///< 打开时传入、未编辑的基线设置。
};

} // namespace lifegame::view
