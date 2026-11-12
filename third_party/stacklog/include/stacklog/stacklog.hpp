#ifndef STACKLOG_STACKLOG_HPP
#define STACKLOG_STACKLOG_HPP

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>

#include <stacklog/version.hpp>

namespace stacklog
{

/// 日志级别；数值越大越严重。
enum class level
{
    debug = 0,
    info = 1,
    warn = 2,
    error = 3,
    off = 4
};

/// 日志运行参数。
struct config
{
    /// 日志目录。
    std::string dir = "logs";
    /// 日志基础名，现行文件为 <base_name>.log。
    std::string base_name = "app";
    /// 单个日志文件的最大字节数。
    std::uint64_t max_file_size = 5ULL * 1024 * 1024;
    /// 保留的历史文件数。
    unsigned max_files = 5;
    /// 运行期最低日志级别。
    level min_level = level::info;
};

/// 跨线程传递的追踪帧快照。
struct trace_frame
{
    /// 顶层链编号；同一次操作的多个步骤共享。
    std::string trace_id;
    /// 当前节点编号；进程内唯一。
    std::string node_id;
    /// 父节点编号；根节点为空。
    std::string parent_id;
    /// 可读作用域名称。
    std::string scope;

    /// 构造空帧。
    trace_frame() = default;

    /// 是否是可用于接续的有效帧。
    bool valid() const noexcept;
};

/// 初始化日志系统；失败时返回 false，日志降级到 stderr。
bool init(const config& cfg = config());

/// 关闭日志文件；可重复调用。
void shutdown();

/// 修改运行期最低日志级别。
void set_level(level value) noexcept;

/// 当前线程上的追踪作用域；构造入栈，析构出栈。
class trace_scope
{
public:
    /// 创建一个追踪节点。
    explicit trace_scope(const std::string& name);

    /// 离开当前追踪节点。
    ~trace_scope() noexcept;

    trace_scope(const trace_scope&) = delete;
    trace_scope& operator=(const trace_scope&) = delete;
};

/// 捕获当前线程栈顶追踪帧的副本，用于跨线程接续。
trace_frame capture_trace();

/// 将外部 trace_frame 接续到当前线程；构造入栈，析构出栈。
/// 须位于作用域最外层，并保证其内所有 trace_scope 先析构。
class restore_scope
{
public:
    /// 接续一个捕获到的帧；无效帧退化为空操作。
    explicit restore_scope(const trace_frame& frame);

    /// 断开接续并出栈。
    ~restore_scope() noexcept;

    restore_scope(const restore_scope&) = delete;
    restore_scope& operator=(const restore_scope&) = delete;

private:
    bool linked_;
};

namespace detail
{

/// 接续外部帧并创建子作用域；成员析构顺序保证 restore 最后出栈。
class restore_trace_scope
{
public:
    /// 先接续 frame，再创建 name 子作用域。
    restore_trace_scope(const trace_frame& frame, const std::string& name);

    /// 先结束 name 子作用域，再断开 frame。
    ~restore_trace_scope() noexcept;

    restore_trace_scope(const restore_trace_scope&) = delete;
    restore_trace_scope& operator=(const restore_trace_scope&) = delete;

private:
    restore_scope restore_;
    trace_scope trace_;
};

/// 流式日志行；析构时落盘。
class log_line
{
public:
    /// 记录级别、文件名和行号。
    log_line(level severity, const char* file, int line);

    /// 写入缓冲内容。
    ~log_line() noexcept;

    log_line(const log_line&) = delete;
    log_line& operator=(const log_line&) = delete;

    /// 追加任意可流式输出的值。
    template <typename T> log_line& operator<<(const T& value)
    {
        if (enabled_)
        {
            stream_ << value;
        }
        return *this;
    }

    /// 支持 std::endl 等流操纵符。
    log_line& operator<<(std::ostream& (*manipulator)(std::ostream&))
    {
        if (enabled_)
        {
            stream_ << manipulator;
        }
        return *this;
    }

private:
    /// 判断某个级别当前是否启用。
    static bool is_enabled(level severity) noexcept;

    /// 写入一行日志；由析构函数调用。
    static void write_line(level severity, const char* file, int line, const std::string& message);

    level severity_;
    const char* file_;
    int line_;
    bool enabled_;
    std::ostringstream stream_;
};

} // namespace detail
} // namespace stacklog

/// 通用日志宏：自动注入文件和行号。
#define STACKLOG_LOG(severity) ::stacklog::detail::log_line((severity), __FILE__, __LINE__)

/// 调试日志。
#define STACKLOG_DEBUG() STACKLOG_LOG(::stacklog::level::debug)
/// 普通信息。
#define STACKLOG_INFO() STACKLOG_LOG(::stacklog::level::info)
/// 警告。
#define STACKLOG_WARN() STACKLOG_LOG(::stacklog::level::warn)
/// 错误。
#define STACKLOG_ERROR() STACKLOG_LOG(::stacklog::level::error)

/// 生成作用域内唯一的追踪变量名。
#define STACKLOG_DETAIL_CONCAT_IMPL(a, b) a##b
#define STACKLOG_DETAIL_CONCAT(a, b) STACKLOG_DETAIL_CONCAT_IMPL(a, b)
#define STACKLOG_DETAIL_UNIQUE(name) STACKLOG_DETAIL_CONCAT(name, __LINE__)

/// 创建当前作用域的追踪节点；同一行不能重复使用，不同行可正常嵌套。
#define STACKLOG_TRACE(name) ::stacklog::trace_scope STACKLOG_DETAIL_UNIQUE(stacklog_trace_scope_)(name)

/// 接续外部帧并立即创建子作用域；等价于 restore_scope + trace_scope。
/// 同一行不能重复使用。
#define STACKLOG_RESTORE_TRACE(frame, name)                                                                            \
    ::stacklog::detail::restore_trace_scope STACKLOG_DETAIL_UNIQUE(stacklog_restore_trace_scope_)((frame), (name))

#endif // STACKLOG_STACKLOG_HPP
