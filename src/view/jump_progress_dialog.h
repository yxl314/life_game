#pragma once

/**
 * @file jump_progress_dialog.h
 * @brief 跳转进度端口的 QProgressDialog 实现。
 */

#include <QPointer>
#include <QWidget>

#include <functional>
#include <memory>

#include "view/interfaces.h"

class QProgressDialog;

namespace lifegame::view
{

/// @brief 用 QProgressDialog 实现 JumpProgressView。
class JumpProgressDialog : public JumpProgressView
{
public:
    /// @brief 构造进度对话框。
    /// @param parent 父控件。
    explicit JumpProgressDialog(QWidget* parent);
    /// @brief 关闭并销毁当前进度对话框。
    ~JumpProgressDialog() override;

    /// @brief 按跳转参数显示进度对话框。
    /// @param info 跳转参数。
    void show(const shared::JumpInfo& info) override;
    /// @brief 更新进度值。
    /// @param done 已重放的代数。
    void setProgress(int done) override;
    /// @brief 程序化关闭，不视为用户取消。
    void close() override;
    /// @brief 注册用户取消时的回调。
    /// @param handler 用户取消时调用的回调。
    void setCancelHandler(std::function<void()> handler) override;

private:
    /// @brief 清空取消回调并关闭、延迟销毁对话框。
    void closeDialog();

    QWidget* m_parent = nullptr; ///< 对话框父窗口。
    QPointer<QProgressDialog> m_dialog; ///< 当前进度对话框；为空表示未显示。
    std::function<void()> m_cancelHandler; ///< 用户取消时调用的回调。
};

} // namespace lifegame::view
