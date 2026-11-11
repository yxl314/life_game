#ifndef SINGLEPROC_SINGLEPROC_HPP
#define SINGLEPROC_SINGLEPROC_HPP

#include <string>

namespace singleproc
{

/// 单实例运行守卫。
///
/// 构造时尝试取得进程级独占锁；一个进程持有锁期间，同 key 的其它进程
/// 会得到 AlreadyRunning。锁由文件锁承担，进程退出或异常终止后由内核释放。
///
/// 守卫不删除锁文件，因为删除锁文件会与其它进程产生 inode 竞争；
/// 锁文件长期存在不代表有实例在运行。
class Guard
{
public:
    /// 本次启动的结果。
    enum class State
    {
        Primary,        // 当前进程取得独占权
        AlreadyRunning, // 同一个 key 已有进程在运行
        Error           // key、目录或锁文件不可用
    };

    /// 尝试取得 key 对应的独占锁。
    ///
    /// Args:
    ///     key: 应用唯一名，只允许字母、数字、'.'、'_'、'-'。
    ///     runtime_dir: 锁文件目录；为空时使用 XDG_RUNTIME_DIR，再退回 /tmp。
    explicit Guard(const std::string& key,
                   const std::string& runtime_dir = std::string());

    /// 释放独占锁。
    ~Guard();

    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;

    /// 返回本次启动的结果。
    State state() const noexcept;

    /// 当前进程是否取得独占权。
    bool isPrimary() const noexcept;

    /// 返回 Error 时的 errno；正常或 AlreadyRunning 时为 0。
    int errorCode() const noexcept;

private:
    int fd_;
    State state_;
    int error_code_;
};

} // namespace singleproc

#endif // SINGLEPROC_SINGLEPROC_HPP
