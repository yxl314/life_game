#pragma once

/**
 * @file image_file_service.h
 * @brief PNG 图像输出端口与服务。
 */

#include <QString>

#include "service/file_error.h"

class QImage;

namespace lifegame::service
{

/// @brief 可替换的 PNG 输出端口。
class ImageFilePort
{
public:
    /// @brief 允许通过端口接口删除实现。
    virtual ~ImageFilePort() = default;

    /**
     * @brief 把图像保存为 PNG。
     * @param image 待编码的图像。
     * @param path 目标路径。
     * @param detail 可选的编码器错误细节。
     * @return 文件错误码。
     */
    virtual FileError savePng(const QImage& image, const QString& path, QString* detail = nullptr) const = 0;
};

/// @brief 基于 QImageWriter 的 PNG 服务。
class ImageFileService : public ImageFilePort
{
public:
    /// @brief 用 PNG 写出器把图像编码到 `path`。
    ///
    /// 空图像在接触文件之前就被拒绝。
    /// @param image 待编码的图像。
    /// @param path 目标路径。
    /// @param detail 可选的编码器错误细节。
    /// @return 文件错误码。
    /// @retval NullImage `image` 为空。
    /// @retval EncodeFailed 写出器拒绝该图像，或文件写不进去。
    FileError savePng(const QImage& image, const QString& path, QString* detail = nullptr) const override;
};

} // namespace lifegame::service
