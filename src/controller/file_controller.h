#pragma once

/**
 * @file file_controller.h
 * @brief RLE 打开/保存与 PNG 导出的控制器。
 */

#include <QObject>
#include <QString>

#include "domain/rle.h"
#include "model/command_result.h"
#include "service/file_error.h"

class QWidget;

namespace lifegame::model
{
class LifeModel;
}

namespace lifegame::service
{
class RleFilePort;
class ImageFilePort;
}

namespace lifegame::view
{
class BoardImageSource;
class MessageSink;
}

namespace lifegame::controller
{

/**
 * @brief RLE 打开/保存与 PNG 导出的控制器。
 *
 * 负责文件对话框并记住上次使用的目录；真正的读写交给注入的端口，因此
 * 本类不含持久化逻辑，可在测试中脱离文件系统驱动。
 */
class FileController : public QObject
{
    Q_OBJECT

public:
    /// @brief 打开一个路径后的文件/解码/模型结果。
    struct OpenResult
    {
        service::FileError fileError = service::FileError::None; ///< I/O 或解码错误。
        domain::RleErrorInfo decodeError;                        ///< RLE 解析细节。
        model::CommandResult modelResult = model::CommandResult::Success; ///< 模型载入结果。
        QString detail;                                          ///< 可选的系统细节。
    };

    /// @brief 绑定模型、窄端口与对话框父窗口。
    /// @param model 可写模型。
    /// @param imageSource PNG 导出用的窄来源。
    /// @param messages 状态栏输出端口。
    /// @param dialogParent 文件对话框的父窗口。
    /// @param rleService RLE 文件端口。
    /// @param imageService PNG 输出端口。
    /// @param parent QObject 父对象。
    FileController(model::LifeModel* model,
                   view::BoardImageSource* imageSource,
                   view::MessageSink* messages,
                   QWidget* dialogParent,
                   service::RleFilePort* rleService,
                   service::ImageFilePort* imageService,
                   QObject* parent = nullptr);

    /// @brief 读取路径，并把解码后的文档交给模型。
    ///
    /// 读取或解码失败时模型保持不变。
    /// @param path RLE 路径。
    /// @return 合并后的打开结果。
    OpenResult openPath(const QString& path);

    /// @brief 把整块棋盘编码后经 RLE 端口写出。
    /// @param path 目标路径。
    /// @param detail 可选的错误细节。
    /// @return 文件错误码。
    service::FileError savePath(const QString& path, QString* detail = nullptr);

    /// @brief 渲染棋盘后经图像端口写出。
    /// @param path PNG 目标路径。
    /// @param detail 可选的错误细节。
    /// @return 文件错误码。
    service::FileError exportPath(const QString& path, QString* detail = nullptr);

public slots:
    /// @brief 弹出打开对话框。
    void openRle();
    /// @brief 弹出保存对话框。
    void saveRle();
    /// @brief 弹出 PNG 导出对话框。
    void exportImage();

private:
    /// @brief 把 RLE 解析失败翻译成本地化文案。
    /// @param error RLE 错误细节。
    /// @return 字符错误带行列号的文案；None 返回空串。
    QString rleErrorMessage(const domain::RleErrorInfo& error) const;

    model::LifeModel* m_model = nullptr;             ///< 可写模型。
    view::BoardImageSource* m_imageSource = nullptr; ///< PNG 导出的窄来源。
    view::MessageSink* m_messages = nullptr;         ///< 状态栏输出端口。
    QWidget* m_dialogParent = nullptr;               ///< 文件对话框的父窗口。
    service::RleFilePort* m_rle = nullptr;           ///< RLE 文件端口。
    service::ImageFilePort* m_image = nullptr;       ///< PNG 输出端口。
    QString m_lastDir;                                ///< 上次对话框使用的目录。
};

} // namespace lifegame::controller
