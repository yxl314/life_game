#include <singleproc/singleproc.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <utility>

namespace
{

constexpr std::size_t max_path_length = 4096;
constexpr int doorbell_connect_timeout_ms = 100;
constexpr int doorbell_backlog = 16;

/// 返回默认锁目录：优先 XDG_RUNTIME_DIR，其次 /tmp。
std::string default_runtime_dir()
{
    const char* value = std::getenv("XDG_RUNTIME_DIR");
    if (value != 0 && value[0] != '\0')
    {
        return std::string(value);
    }
    return std::string("/tmp");
}

/// 判断 key 是否能安全地作为文件名的一部分。
bool valid_key(const std::string& key)
{
    if (key.empty())
    {
        return false;
    }
    for (std::string::const_iterator iter = key.begin(); iter != key.end(); ++iter)
    {
        const char value = *iter;
        const bool alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        const bool digit = value >= '0' && value <= '9';
        if (!alpha && !digit && value != '.' && value != '_' && value != '-')
        {
            return false;
        }
    }
    return true;
}

/// 目录字符串中不能嵌入 NUL，否则路径会被系统调用静默截断。
bool valid_directory(const std::string& directory)
{
    return directory.find('\0') == std::string::npos;
}

/// 拼接目录和文件名；目录为空时使用默认目录。
std::string join_path(const std::string& directory, const std::string& name)
{
    std::string base = directory;
    if (base.empty())
    {
        base = default_runtime_dir();
    }
    if (base[base.size() - 1] != '/')
    {
        base += '/';
    }
    return base + name;
}

/// 生成锁文件路径。
std::string make_lock_path(const std::string& directory, const std::string& key)
{
    const std::string name =
        "singleproc-" + std::to_string(static_cast<unsigned long>(::getuid())) + "-" + key + ".lock";
    return join_path(directory, name);
}

/// 生成门铃 socket 路径。
std::string make_socket_path(const std::string& directory, const std::string& key)
{
    const std::string name =
        "singleproc-" + std::to_string(static_cast<unsigned long>(::getuid())) + "-" + key + ".sock";
    return join_path(directory, name);
}

/// 把路径转换成 Unix domain socket 地址。
bool make_unix_address(const std::string& path, sockaddr_un& address)
{
    if (path.empty() || path.size() >= sizeof(address.sun_path))
    {
        return false;
    }
    std::memset(&address, 0, sizeof(address));
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return true;
}

void set_close_on_exec(int fd)
{
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags >= 0)
    {
        (void)::fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
    }
}

void set_nonblocking(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags >= 0)
    {
        (void)::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

/// 二次实例：尝试一次短连接，把固定的单字节通知送给主实例。
void notify_doorbell(const std::string& socket_path)
{
    sockaddr_un address;
    if (!make_unix_address(socket_path, address))
    {
        return;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return;
    }
    set_close_on_exec(fd);
    set_nonblocking(fd);

    const int result = ::connect(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    if (result != 0 && errno != EINPROGRESS)
    {
        ::close(fd);
        return;
    }
    if (result != 0)
    {
        pollfd descriptor;
        descriptor.fd = fd;
        descriptor.events = POLLOUT;
        descriptor.revents = 0;
        if (::poll(&descriptor, 1, doorbell_connect_timeout_ms) <= 0)
        {
            ::close(fd);
            return;
        }

        int error = 0;
        socklen_t length = sizeof(error);
        if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) != 0 || error != 0)
        {
            ::close(fd);
            return;
        }
    }

    const char marker = 1;
    int send_flags = 0;
#ifdef MSG_NOSIGNAL
    send_flags |= MSG_NOSIGNAL;
#endif
    (void)::send(fd, &marker, 1, send_flags);
    ::close(fd);
}

/// 主实例：取得文件锁后创建并监听门铃 socket。
/// 返回监听 fd；任何一步失败都返回 -1，由调用方保持门铃禁用。
int start_doorbell(const std::string& socket_path)
{
    sockaddr_un address;
    if (!make_unix_address(socket_path, address))
    {
        return -1;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return -1;
    }
    set_close_on_exec(fd);
    set_nonblocking(fd);

    // 已经持有文件锁，同名 socket 只可能是上次异常终止留下的残留。
    if (::unlink(socket_path.c_str()) != 0 && errno != ENOENT)
    {
        ::close(fd);
        return -1;
    }

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        ::close(fd);
        return -1;
    }

    if (::chmod(socket_path.c_str(), 0600) != 0)
    {
        ::close(fd);
        (void)::unlink(socket_path.c_str());
        return -1;
    }

    if (::listen(fd, doorbell_backlog) != 0)
    {
        ::close(fd);
        (void)::unlink(socket_path.c_str());
        return -1;
    }

    return fd;
}

} // namespace

namespace singleproc
{

Guard::Guard(const std::string& key, const std::string& runtime_dir)
    : fd_(-1), state_(State::Error), error_code_(0), doorbell_fd_(-1), doorbell_pending_(false)
{
    if (!valid_key(key) || !valid_directory(runtime_dir))
    {
        error_code_ = EINVAL;
        return;
    }

    const std::string lock_path = make_lock_path(runtime_dir, key);
    if (lock_path.empty() || lock_path.size() >= max_path_length)
    {
        error_code_ = ENAMETOOLONG;
        return;
    }

    socket_path_ = make_socket_path(runtime_dir, key);
    sockaddr_un socket_address;
    if (!make_unix_address(socket_path_, socket_address))
    {
        // socket 路径超出平台限制时只禁门铃，单实例锁仍然有效。
        socket_path_.clear();
    }

    int flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    const int fd = ::open(lock_path.c_str(), flags, 0600);
    if (fd < 0)
    {
        error_code_ = errno;
        return;
    }

    struct stat file_status;
    if (::fstat(fd, &file_status) != 0)
    {
        const int error = errno;
        ::close(fd);
        error_code_ = error;
        return;
    }

    if (!S_ISREG(file_status.st_mode))
    {
        ::close(fd);
        error_code_ = EINVAL;
        return;
    }

    if (file_status.st_uid != ::getuid())
    {
        ::close(fd);
        error_code_ = EACCES;
        return;
    }

    if (::fchmod(fd, 0600) != 0)
    {
        const int error = errno;
        ::close(fd);
        error_code_ = error;
        return;
    }

    if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
    {
        const int error = errno;
        ::close(fd);
        if (error == EWOULDBLOCK || error == EAGAIN)
        {
            state_ = State::AlreadyRunning;
            error_code_ = 0;
            notify_doorbell(socket_path_);
        }
        else
        {
            error_code_ = error;
        }
        return;
    }

    fd_ = fd;
    state_ = State::Primary;
    error_code_ = 0;

    if (!socket_path_.empty())
    {
        doorbell_fd_ = start_doorbell(socket_path_);
    }
}

Guard::~Guard()
{
    if (doorbell_fd_ >= 0)
    {
        ::close(doorbell_fd_);
        doorbell_fd_ = -1;
        if (!socket_path_.empty())
        {
            (void)::unlink(socket_path_.c_str());
        }
    }

    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

Guard::State Guard::state() const noexcept
{
    return state_;
}

bool Guard::is_primary() const noexcept
{
    return state_ == State::Primary;
}

int Guard::error_code() const noexcept
{
    return error_code_;
}

void Guard::set_doorbell_handler(DoorbellHandler handler)
{
    doorbell_handler_ = std::move(handler);
}

bool Guard::process_doorbell(int timeout_ms)
{
    if (doorbell_fd_ < 0)
    {
        return false;
    }

    if (!doorbell_pending_)
    {
        pollfd descriptor;
        descriptor.fd = doorbell_fd_;
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        if (::poll(&descriptor, 1, timeout_ms) <= 0)
        {
            return false;
        }
    }

    for (;;)
    {
        const int client = ::accept(doorbell_fd_, 0, 0);
        if (client < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        set_close_on_exec(client);
        ::close(client);
        doorbell_pending_ = true;
    }

    if (!doorbell_pending_)
    {
        return false;
    }

    // 复制一份再调用：handler 内替换自己时不会销毁正在执行的函数对象。
    const DoorbellHandler handler = doorbell_handler_;
    if (!handler)
    {
        return false;
    }

    doorbell_pending_ = false;
    handler();
    return true;
}

int Guard::doorbell_fd() const noexcept
{
    return doorbell_fd_;
}

} // namespace singleproc
