#pragma once

/**
 * @file interfaces.h
 * @brief 供控制器使用的窄视图输出端口。
 */

#include <QString>

#include <functional>
#include <memory>

#include "shared/jump_info.h"

class QImage;

namespace lifegame::view
{

/// @brief 导出当前棋盘图像的窄输出端口。
class BoardImageSource
{
public:
    /// @brief 释放端口对象。
    virtual ~BoardImageSource() = default;

    /// @brief 渲染当前棋盘的完整图像。
    /// @return 完整棋盘图像；没有棋盘时返回空图像。
    virtual QImage renderBoardImage() const = 0;
};

/// @brief 帧调度使用的窄指标来源。
class FrameTimingSource
{
public:
    /// @brief 释放端口对象。
    virtual ~FrameTimingSource() = default;

    /// @brief 返回上一帧的绘制耗时。
    /// @return 毫秒数。
    virtual double lastPaintDurationMs() const = 0;
};

/// @brief 破坏性操作确认的输出端口。
class ConfirmSink
{
public:
    /// @brief 释放端口对象。
    virtual ~ConfirmSink() = default;

    /// @brief 请求用户确认破坏性操作。
    /// @param title 对话框标题。
    /// @param text 确认正文。
    /// @return 用户接受时返回 true。
    virtual bool confirm(const QString& title, const QString& text) = 0;
};

/// @brief 临时状态文本的输出端口。
class MessageSink
{
public:
    /// @brief 释放端口对象。
    virtual ~MessageSink() = default;

    /// @brief 显示一条临时状态文本。
    /// @param text 要显示的消息。
    /// @param timeoutMs 自动隐藏的毫秒数；0 表示常驻。
    virtual void showMessage(const QString& text, int timeoutMs) = 0;
};

/// @brief 跳转进度展示的输出端口。
class JumpProgressView
{
public:
    /// @brief 释放端口对象。
    virtual ~JumpProgressView() = default;

    /// @brief 显示跳转进度。
    /// @param info 跳转参数。
    virtual void show(const shared::JumpInfo& info) = 0;

    /// @brief 更新已完成的代数。
    /// @param done 已重放的代数。
    virtual void setProgress(int done) = 0;

    /// @brief 关闭进度视图。
    virtual void close() = 0;

    /// @brief 注册用户取消时的回调。
    /// @param handler 用户取消时调用的回调。
    virtual void setCancelHandler(std::function<void()> handler) = 0;
};

/// @brief 创建跳转进度视图的工厂。
using JumpProgressFactory = std::function<std::unique_ptr<JumpProgressView>()>;

} // namespace lifegame::view
