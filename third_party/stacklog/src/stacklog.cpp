#include "stacklog/stacklog.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

namespace stacklog
{
namespace
{

/// 线程内追踪栈、节点序号和线程注册序号。
struct TraceContext
{
    std::vector<TraceFrame> stack;
    std::uint64_t           node_sequence     = 0;
    std::uint64_t           thread_sequence   = 0;
    bool                    thread_registered = false;
};

/// 进程级 trace 序号。
std::atomic<std::uint64_t> g_trace_sequence(0);

/// 进程级线程注册序号。
std::atomic<std::uint64_t> g_thread_sequence(0);

/// 运行期最低级别。
std::atomic<int> g_level(static_cast<int>(Level::Info));

/// 未初始化降级提示只打印一次。
std::atomic<int> g_fallback_hinted(0);

/// 日志器共享状态。
struct LoggerState
{
    std::mutex    mutex;
    std::ofstream file;
    Config        config;
    std::string   path;
    std::uint64_t written_bytes = 0;
    bool          ready         = false;
};

/// 进程常驻状态；不析构，避免静态析构顺序问题。
LoggerState& state()
{
    static LoggerState* instance = new LoggerState;
    return *instance;
}

std::string escape_field(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (std::string::const_iterator it = value.begin(); it != value.end(); ++it)
    {
        switch (*it)
        {
        case '\\':
            result += "\\\\";
            break;
        case '|':
            result += "\\|";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\n':
            result += "\\n";
            break;
        default:
            result += *it;
            break;
        }
    }
    return result;
}

std::string dash_if_empty(const std::string& value)
{
    return value.empty() ? std::string("-") : value;
}

const char* level_name(Level level)
{
    switch (level)
    {
    case Level::Debug:
        return "DEBUG";
    case Level::Info:
        return "INFO";
    case Level::Warn:
        return "WARN";
    case Level::Error:
        return "ERROR";
    case Level::Off:
        return "OFF";
    }
    return "UNKNOWN";
}

std::string base_name(const char* file)
{
    if (NULL == file)
    {
        return "-";
    }
    const char* slash = std::strrchr(file, '/');
    const char* back  = std::strrchr(file, '\\');
    if (NULL != back && (NULL == slash || back > slash))
    {
        slash = back;
    }
    return (NULL == slash) ? std::string(file) : std::string(slash + 1);
}

std::string timestamp()
{
    const std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    const long long milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;

    std::tm tm_value;
    std::memset(&tm_value, 0, sizeof(tm_value));
#ifdef _WIN32
    localtime_s(&tm_value, &seconds);
#else
    localtime_r(&seconds, &tm_value);
#endif

    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_value);
    char result[40];
    std::snprintf(result, sizeof(result), "%s.%03lld", buffer, milliseconds);
    return std::string(result);
}

std::string join_path(const std::string& dir, const std::string& name)
{
    if (dir.empty())
    {
        return name;
    }
    const char last = dir[dir.size() - 1];
    if (last == '/' || last == '\\')
    {
        return dir + name;
    }
    return dir + "/" + name;
}

bool make_one_dir(const std::string& path)
{
    if (path.empty())
    {
        return true;
    }
#ifdef _WIN32
    const int result = _mkdir(path.c_str());
#else
    const int result = ::mkdir(path.c_str(), 0755);
#endif
    return result == 0 || errno == EEXIST;
}

bool make_dirs(const std::string& dir)
{
    if (dir.empty() || dir == ".")
    {
        return true;
    }

    // 逐级创建父目录；跳过根目录和 Windows 盘符。
    for (std::string::size_type i = 1; i < dir.size(); ++i)
    {
        const char ch = dir[i];
        if (ch != '/' && ch != '\\')
        {
            continue;
        }
        const std::string parent = dir.substr(0, i);
        if (parent.empty() || parent == "/" ||
            (parent.size() == 2 && parent[1] == ':'))
        {
            continue;
        }
        if (!make_one_dir(parent))
        {
            return false;
        }
    }
    return make_one_dir(dir);
}

TraceContext& trace_context()
{
    static thread_local TraceContext context;
    return context;
}

/// 给每个线程分配进程内唯一的注册序号。
std::uint64_t thread_sequence(TraceContext& context)
{
    if (!context.thread_registered)
    {
        context.thread_sequence = g_thread_sequence.fetch_add(1) + 1;
        context.thread_registered = true;
    }
    return context.thread_sequence;
}

TraceFrame* current_frame()
{
    TraceContext& context = trace_context();
    return context.stack.empty() ? NULL : &context.stack.back();
}

std::string make_trace_id()
{
    const std::uint64_t sequence = g_trace_sequence.fetch_add(1) + 1;
    const long long milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    return "t-" + std::to_string(milliseconds) + "-" + std::to_string(sequence);
}

void begin_trace(const std::string& name)
{
    TraceContext& context = trace_context();
    TraceFrame frame;
    frame.scope = name.empty() ? std::string("unnamed") : name;

    if (context.stack.empty())
    {
        frame.trace_id = make_trace_id();
    }
    else
    {
        frame.trace_id  = context.stack.back().trace_id;
        frame.parent_id = context.stack.back().node_id;
    }

    // 节点号加入线程注册序号，保证跨线程接续后仍进程内唯一。
    const std::uint64_t thread_id = thread_sequence(context);
    ++context.node_sequence;
    frame.node_id = frame.trace_id + "-t" + std::to_string(thread_id) + "-" +
                    std::to_string(context.node_sequence);
    context.stack.push_back(frame);
}

void end_trace()
{
    TraceContext& context = trace_context();
    if (!context.stack.empty())
    {
        context.stack.pop_back();
    }
}

void restore_trace(const TraceFrame& frame)
{
    if (frame.valid())
    {
        trace_context().stack.push_back(frame);
    }
}

std::string format_line(
    Level level, const TraceFrame* frame, const std::string& message, const char* file, int line)
{
    const std::string scope_name = (NULL == frame) ? std::string("-") : escape_field(dash_if_empty(frame->scope));
    const std::string trace_id   = (NULL == frame) ? std::string("-") : escape_field(dash_if_empty(frame->trace_id));
    const std::string node_id    = (NULL == frame) ? std::string("-") : escape_field(dash_if_empty(frame->node_id));
    const std::string parent_id  = (NULL == frame) ? std::string("-") : escape_field(dash_if_empty(frame->parent_id));
    const std::string location   = escape_field(base_name(file) + ":" + std::to_string(line));

    std::string result;
    result += timestamp();
    result += "|";
    result += level_name(level);
    result += "|";
    result += trace_id;
    result += "|";
    result += node_id;
    result += "|";
    result += parent_id;
    result += "|";
    result += scope_name;
    result += "|";
    result += location;
    result += "|";
    result += escape_field(message);
    return result;
}

void rotate_locked(LoggerState& logger)
{
    logger.file.flush();
    logger.file.close();

    const std::string base = logger.path;
    if (logger.config.max_files > 0)
    {
        std::remove((base + "." + std::to_string(logger.config.max_files)).c_str());
        for (unsigned index = logger.config.max_files; index > 1; --index)
        {
            const std::string from = base + "." + std::to_string(index - 1);
            const std::string to   = base + "." + std::to_string(index);
            std::rename(from.c_str(), to.c_str());
        }
        std::rename(base.c_str(), (base + ".1").c_str());
    }

    logger.written_bytes = 0;
    logger.file.open(logger.path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    if (!logger.file.is_open())
    {
        logger.ready = false;
    }
}

void fallback_to_stderr(const std::string& line)
{
    if (0 == g_fallback_hinted.fetch_add(1))
    {
        std::fprintf(stderr, "stacklog: 文件输出未就绪，日志降级到 stderr\n");
    }
    std::fprintf(stderr, "%s\n", line.c_str());
}

} // namespace

Config::Config()
    : dir("logs"),
      base_name("app"),
      max_file_size(5 * 1024 * 1024),
      max_files(5),
      level(Level::Info)
{
}

bool init(const Config& config)
{
    if (config.dir.empty() || config.base_name.empty() || config.max_file_size == 0 ||
        config.max_files == 0)
    {
        std::fprintf(stderr, "stacklog: 配置非法，初始化失败\n");
        return false;
    }

    LoggerState& logger = state();
    std::lock_guard<std::mutex> lock(logger.mutex);

    logger.file.close();
    logger.ready = false;
    logger.config = config;
    logger.path = join_path(config.dir, config.base_name + ".log");
    logger.written_bytes = 0;

    if (!make_dirs(config.dir))
    {
        std::fprintf(stderr, "stacklog: 无法创建日志目录: %s\n", config.dir.c_str());
        return false;
    }

    std::ifstream existing(logger.path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if (existing.is_open())
    {
        const std::ifstream::pos_type size = existing.tellg();
        if (size > 0)
        {
            logger.written_bytes = static_cast<std::uint64_t>(size);
        }
    }
    existing.close();

    logger.file.open(logger.path.c_str(), std::ios::out | std::ios::app | std::ios::binary);
    if (!logger.file.is_open())
    {
        std::fprintf(stderr, "stacklog: 无法打开日志文件: %s\n", logger.path.c_str());
        return false;
    }

    logger.ready = true;
    g_level.store(static_cast<int>(config.level));
    return true;
}

void shutdown()
{
    LoggerState& logger = state();
    std::lock_guard<std::mutex> lock(logger.mutex);
    if (logger.file.is_open())
    {
        logger.file.flush();
        logger.file.close();
    }
    logger.ready = false;
}

void setLevel(Level level)
{
    g_level.store(static_cast<int>(level));
}

bool detail::enabled(Level level)
{
    const int configured = g_level.load();
    if (configured == static_cast<int>(Level::Off) || level == Level::Off)
    {
        return false;
    }
    return static_cast<int>(level) >= configured;
}

void detail::write(Level level, const char* file, int line, const std::string& message)
{
    if (!enabled(level))
    {
        return;
    }

    const TraceFrame* frame = current_frame();
    const std::string text = format_line(level, frame, message, file, line);

    LoggerState& logger = state();
    bool ready = false;
    bool written = false;
    {
        std::lock_guard<std::mutex> lock(logger.mutex);
        ready = logger.ready;
        if (ready)
        {
            const std::uint64_t required = static_cast<std::uint64_t>(text.size() + 1);
            if (logger.written_bytes + required > logger.config.max_file_size)
            {
                rotate_locked(logger);
            }

            if (logger.ready && logger.file.is_open())
            {
                logger.file.write(text.c_str(), static_cast<std::streamsize>(text.size()));
                logger.file.put('\n');
                logger.file.flush();
                if (logger.file.good())
                {
                    logger.written_bytes += required;
                    written = true;
                }
                else
                {
                    logger.ready = false;
                }
            }
        }
    }

    if (!ready || !written)
    {
        fallback_to_stderr(text);
    }
}

TraceScope::TraceScope(const std::string& name)
{
    begin_trace(name);
}

TraceScope::~TraceScope()
{
    end_trace();
}

TraceFrame::TraceFrame()
{
}

bool TraceFrame::valid() const
{
    return !trace_id.empty() && !node_id.empty();
}

TraceFrame captureTrace()
{
    const TraceFrame* frame = current_frame();
    return (NULL == frame) ? TraceFrame() : *frame;
}

RestoreScope::RestoreScope(const TraceFrame& frame)
    : m_linked(frame.valid())
{
    if (m_linked)
    {
        restore_trace(frame);
    }
}

RestoreScope::~RestoreScope()
{
    if (m_linked)
    {
        end_trace();
    }
}

detail::LogLine::LogLine(Level level, const char* file, int line)
    : m_level(level),
      m_file(file),
      m_line(line),
      m_enabled(detail::enabled(level))
{
}

detail::LogLine::~LogLine()
{
    if (m_enabled)
    {
        detail::write(m_level, m_file, m_line, m_stream.str());
    }
}

} // namespace stacklog
