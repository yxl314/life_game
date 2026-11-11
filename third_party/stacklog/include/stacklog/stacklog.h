#ifndef STACKLOG_STACKLOG_H
#define STACKLOG_STACKLOG_H

#include <cstdint>
#include <ostream>
#include <sstream>
#include <string>

#include "stacklog/version.h"

namespace stacklog
{

/// 日志级别；数值越大越严重。
enum class Level
{
    Debug = 0,
    Info  = 1,
    Warn  = 2,
    Error = 3,
    Off   = 4
};

/// 日志运行参数。
struct Config
{
    /// 日志目录。
    std::string dir;
    /// 日志基础名，现行文件为 <base_name>.log。
    std::string base_name;
    /// 单个日志文件的最大字节数。
    std::uint64_t max_file_size;
    /// 保留的历史文件数。
    unsigned max_files;
    /// 运行期最低日志级别。
    Level level;

    /// 使用默认参数：logs、app、5 MiB、5 份、Info。
    Config();
};

/// 跨线程传递的追踪帧快照。
struct TraceFrame
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
    TraceFrame();

    /// 是否是可用于接续的有效帧。
    bool valid() const;
};

/// 初始化日志系统；失败时返回 false，日志降级到 stderr。
bool init(const Config& config = Config());

/// 关闭日志文件；可重复调用。
void shutdown();

/// 修改运行期最低日志级别。
void setLevel(Level level);

/// 当前线程上的追踪作用域；构造入栈，析构出栈。
class TraceScope
{
public:
    /// 创建一个追踪节点。
    explicit TraceScope(const std::string& name);

    /// 离开当前追踪节点。
    ~TraceScope();

    TraceScope(const TraceScope&)            = delete;
    TraceScope& operator=(const TraceScope&) = delete;
};

/// 捕获当前线程栈顶追踪帧的副本，用于跨线程接续。
TraceFrame captureTrace();

/// 将外部 TraceFrame 接续到当前线程；构造入栈，析构出栈。
/// 须位于作用域最外层，并保证其内所有 TraceScope 先析构。
class RestoreScope
{
public:
    /// 接续一个捕获到的帧；无效帧退化为空操作。
    explicit RestoreScope(const TraceFrame& frame);

    /// 断开接续并出栈。
    ~RestoreScope();

    RestoreScope(const RestoreScope&)            = delete;
    RestoreScope& operator=(const RestoreScope&) = delete;

private:
    bool m_linked;
};

namespace detail
{

/// 判断某个级别当前是否启用。
bool enabled(Level level);

/// 写入一行日志；由 LogLine 析构时调用。
void write(Level level, const char* file, int line, const std::string& message);

/// 流式日志行；析构时落盘。
class LogLine
{
public:
    /// 记录级别、文件名和行号。
    LogLine(Level level, const char* file, int line);

    /// 写入缓冲内容。
    ~LogLine();

    LogLine(const LogLine&)            = delete;
    LogLine& operator=(const LogLine&) = delete;

    /// 追加任意可流式输出的值。
    template <typename T>
    LogLine& operator<<(const T& value)
    {
        if (m_enabled)
        {
            m_stream << value;
        }
        return *this;
    }

    /// 支持 std::endl 等流操纵符。
    LogLine& operator<<(std::ostream& (*manipulator)(std::ostream&))
    {
        if (m_enabled)
        {
            m_stream << manipulator;
        }
        return *this;
    }

private:
    Level              m_level;
    const char*        m_file;
    int                m_line;
    bool               m_enabled;
    std::ostringstream m_stream;
};

} // namespace detail
} // namespace stacklog

/// 通用日志宏：自动注入文件和行号。
#define STACKLOG_LOG(level) ::stacklog::detail::LogLine((level), __FILE__, __LINE__)

/// 调试日志。
#define STACKLOG_DEBUG() STACKLOG_LOG(::stacklog::Level::Debug)
/// 普通信息。
#define STACKLOG_INFO() STACKLOG_LOG(::stacklog::Level::Info)
/// 警告。
#define STACKLOG_WARN() STACKLOG_LOG(::stacklog::Level::Warn)
/// 错误。
#define STACKLOG_ERROR() STACKLOG_LOG(::stacklog::Level::Error)

/// 创建当前作用域的追踪节点；同一作用域只能使用一次。
#define STACKLOG_TRACE(name) ::stacklog::TraceScope stacklog_trace_scope_(name)

/// 接续外部帧并立即创建子作用域；等价于 RestoreScope + TraceScope。
/// 同一作用域只能使用一次，须放在作用域起始处。
#define STACKLOG_RESTORE_TRACE(frame, name)                                                                             \
    ::stacklog::RestoreScope stacklog_restore_scope_((frame));                                                         \
    ::stacklog::TraceScope stacklog_trace_scope_((name))

#endif // STACKLOG_STACKLOG_H
