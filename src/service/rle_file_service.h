#pragma once

/**
 * @file rle_file_service.h
 * @brief RLE 文件读写端口与服务。
 */

#include <QString>

#include <optional>

#include "domain/rle.h"
#include "service/file_error.h"

namespace lifegame::service
{

/// @brief 读取并解码 RLE 文件的结果。
struct RleReadResult
{
    std::optional<domain::DecodedDocument> document; ///< 成功时的解码文档。
    FileError error = FileError::None;               ///< 类型化文件错误。
    domain::RleErrorInfo decodeError;                ///< 解码失败时的 RLE 解析细节。
    QString detail;                                  ///< 可选的系统细节。
};

/// @brief 可替换的 RLE 文件读写端口。
class RleFilePort
{
public:
    /// @brief 允许通过端口接口删除实现。
    virtual ~RleFilePort() = default;

    /**
     * @brief 读取文件并按 RLE 解码。
     *
     * @param path 文件路径。
     * @return 解码文档，或类型化的文件/解码错误。
     */
    virtual RleReadResult read(const QString& path) const = 0;

    /**
     * @brief 原子地写入 RLE 文本。
     * @param path 目标路径。
     * @param text RLE 文本。
     * @param detail 可选的系统错误细节。
     * @return 文件错误码。
     */
    virtual FileError write(const QString& path, const QString& text, QString* detail = nullptr) const = 0;
};

/// @brief 只做文件读写；RLE 解析交给纯领域编解码器。
class RleFileService : public RleFilePort
{
public:
    /// @brief 最多读 8 MiB，再把字节按 RLE 解码。
    ///
    /// 上限在读的过程中执行，而不依赖 file.size()——管道等特殊文件的
    /// file.size() 是 -1。
    /// @param path 文件路径。
    /// @return 解码文档，或类型化的文件/解码错误。
    /// @retval OpenFailed 文件无法以只读方式打开。
    /// @retval TooLarge 文件超过 8 MiB。
    /// @retval ReadFailed 读取本身失败。
    /// @retval DecodeFailed 字节不是合法的 RLE 文档。
    RleReadResult read(const QString& path) const override;

    /// @brief 经 QSaveFile 写 UTF-8 文本，写失败时原文件保持不变。
    /// @param path 目标路径。
    /// @param text RLE 文本。
    /// @param detail 可选的系统错误细节。
    /// @return 文件错误码。
    /// @retval OpenFailed 临时文件创建失败。
    /// @retval WriteFailed 写入或提交临时文件失败。
    FileError write(const QString& path, const QString& text, QString* detail = nullptr) const override;
};

} // namespace lifegame::service
