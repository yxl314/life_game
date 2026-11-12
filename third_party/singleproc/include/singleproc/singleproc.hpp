#ifndef SINGLEPROC_SINGLEPROC_HPP
#define SINGLEPROC_SINGLEPROC_HPP

#include <functional>
#include <string>

namespace singleproc
{

/// 二次启动通知的回调类型。
///
/// 回调在调用 process_doorbell() 的线程中同步执行，必须保持简短。
using DoorbellHandler = std::function<void()>;

/// 单实例运行守卫。
///
/// 构造时尝试取得进程级独占锁；一个进程持有锁期间，同 key 的其它进程
/// 会得到 AlreadyRunning。锁由文件锁承担，进程退出或异常终止后由内核释放。
///
/// 守卫不删除锁文件，因为删除锁文件会与其它进程产生 inode 竞争；
/// 锁文件长期存在不代表有实例在运行。
///
/// 主实例还可以启用二次启动门铃：二次实例在得到 AlreadyRunning 后向
/// 主实例发送一次无载荷通知，主实例通过 process_doorbell() 分发回调。
/// 门铃失败不会改变 state() 或 error_code()。
///
/// Guard 的方法不是线程安全的；门铃回调也不在库内部线程中执行。
class Guard
{
public:
    /// 本次启动的结果。
    enum class State
    {
        /// 当前进程取得独占权。
        Primary,

        /// 同一个 key 已有进程在运行。
        AlreadyRunning,

        /// key、目录或锁文件不可用。
        Error
    };

    /// 尝试取得 key 对应的独占锁。
    ///
    /// Args:
    ///     key: 应用唯一名，只允许字母、数字、'.'、'_'、'-'。
    ///     runtime_dir: 锁文件和门铃 socket 的目录；为空时使用
    ///         XDG_RUNTIME_DIR，再退回 /tmp。
    explicit Guard(const std::string& key, const std::string& runtime_dir = std::string());

    /// 关闭门铃监听、删除 socket 文件并释放独占锁。
    ~Guard();

    // 解锁和 socket 生命周期都绑定在对象上，拷贝/移动会让所有权含糊。
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    Guard(Guard&&) = delete;
    Guard& operator=(Guard&&) = delete;

    /// 返回本次启动的结果。
    State state() const noexcept;

    /// 当前进程是否取得独占权。
    ///
    /// Error 也返回 false；需要区分“已有实例”和“锁失败”时请查 state()。
    bool is_primary() const noexcept;

    /// 返回 Error 时的 errno；正常或 AlreadyRunning 时为 0。
    int error_code() const noexcept;

    /// 注册二次启动通知的回调；替换旧回调。
    ///
    /// 回调为空时保留待处理通知，等待之后的 process_doorbell() 再分发。
    void set_doorbell_handler(DoorbellHandler handler);

    /// 处理待处理的二次启动通知。
    ///
    /// timeout_ms 为 0 时非阻塞；小于 0 时等待到有通知为止。
    /// 门铃不可用或未设置 handler 时立即返回 false。
    /// 返回 true 表示本次调用至少执行了一次回调。
    /// handler 抛出异常时，本次通知已消费，不会重放。
    /// 文件锁的关系不变：门铃失败不会把 Primary 变成 Error。
    bool process_doorbell(int timeout_ms = 0);

    /// 返回门铃监听 fd；门铃不可用时返回 -1。
    ///
    /// 这是借用 fd：归 Guard 所有，有效至 Guard 析构；调用方不得 close
    /// 或修改它。可以交给 poll、select 或 Qt QSocketNotifier 监听；
    /// fd 可读时再调用 process_doorbell(0)，不要直接读取该 fd。
    int doorbell_fd() const noexcept;

private:
    int fd_ = -1;
    State state_ = State::Error;
    int error_code_ = 0;
    std::string socket_path_;
    int doorbell_fd_ = -1;
    DoorbellHandler doorbell_handler_;
    bool doorbell_pending_ = false;
};

} // namespace singleproc

#endif // SINGLEPROC_SINGLEPROC_HPP
