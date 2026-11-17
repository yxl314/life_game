// 本文件只在 POSIX 构建中编译。整文件加平台守卫，Windows 门禁里的 clang-tidy
// 会把它当成空翻译单元，不会因为找不到 POSIX 头文件误报。
#ifndef _WIN32

/// @file
/// @brief Guard 的 POSIX 平台实现：flock 文件锁与 Unix domain socket 门铃。

#include "platform.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <new>
#include <poll.h>
#include <string>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

namespace singleproc
{
namespace detail
{

namespace
{

constexpr int doorbell_connect_timeout_ms = 100; // 本地 socket 的短连接上限
constexpr int doorbell_backlog = 16; // 通知会合并，不需要很大的队列

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

bool set_close_on_exec(int fd)
{
    const int flags = ::fcntl(fd, F_GETFD);
    if (flags < 0)
    {
        return false;
    }
    return ::fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

bool set_nonblocking(int fd)
{
    const int flags = ::fcntl(fd, F_GETFL);
    if (flags < 0)
    {
        return false;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

} // namespace

/// @brief 门铃监听端：监听 fd 与 socket 路径。
///
/// 路径用定长缓冲保存：socket 路径已经过 sun_path 长度校验，固定数组足够，
/// 且构造过程不再分配内存，避免“已取得文件锁后抛 bad_alloc 导致 fd 泄漏”。
struct Doorbell
{
    /// @brief 监听 fd。
    int fd = -1;

    /// @brief socket 路径，析构时删除残留文件。
    char path[sizeof(sockaddr_un)] = {};
};

char path_separator()
{
    return '/';
}

std::string default_runtime_dir()
{
    const char* value = std::getenv("XDG_RUNTIME_DIR");
    if (value != nullptr && value[0] != '\0')
    {
        return {value};
    }
    return {"/tmp"};
}

std::string user_identifier()
{
    return std::to_string(static_cast<unsigned long>(::getuid()));
}

LockAttempt acquire_lock(const std::string& path)
{
    LockAttempt attempt;

    int flags = O_RDWR | O_CREAT;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    const int fd = ::open(path.c_str(), flags, 0600);
    if (fd < 0)
    {
        attempt.error_code = errno;
        return attempt;
    }

    // O_CLOEXEC 在部分平台可能缺失；fcntl 作为统一兜底。
    if (!set_close_on_exec(fd))
    {
        const int error = errno;
        ::close(fd);
        attempt.error_code = error;
        return attempt;
    }

    struct stat file_status;
    if (::fstat(fd, &file_status) != 0)
    {
        const int error = errno;
        ::close(fd);
        attempt.error_code = error;
        return attempt;
    }

    if (!S_ISREG(file_status.st_mode))
    {
        ::close(fd);
        attempt.error_code = EINVAL;
        return attempt;
    }

    if (file_status.st_uid != ::getuid())
    {
        ::close(fd);
        attempt.error_code = EACCES;
        return attempt;
    }

    if (::fchmod(fd, 0600) != 0)
    {
        const int error = errno;
        ::close(fd);
        attempt.error_code = error;
        return attempt;
    }

    if (::flock(fd, LOCK_EX | LOCK_NB) != 0)
    {
        const int error = errno;
        ::close(fd);
        // Linux 上 EWOULDBLOCK 与 EAGAIN 相等；分开判断会触发 -Wlogical-op。
        if (error == EAGAIN || (EWOULDBLOCK != EAGAIN && error == EWOULDBLOCK))
        {
            attempt.result = LockResult::AlreadyRunning;
            attempt.error_code = 0;
        }
        else
        {
            attempt.error_code = error;
        }
        return attempt;
    }

    attempt.result = LockResult::Primary;
    attempt.handle = static_cast<LockHandle>(fd);
    return attempt;
}

void release_lock(LockHandle handle)
{
    if (handle == invalid_lock_handle)
    {
        return;
    }
    (void)::close(static_cast<int>(handle));
}

std::string make_doorbell_target(const std::string& runtime_dir, const std::string& runtime_name)
{
    return join_path(runtime_dir, runtime_name + ".sock");
}

bool valid_doorbell_target(const std::string& target)
{
    sockaddr_un address;
    return make_unix_address(target, address);
}

Doorbell* start_doorbell(const std::string& target)
{
    sockaddr_un address;
    if (!make_unix_address(target, address))
    {
        return nullptr;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return nullptr;
    }
    if (!set_close_on_exec(fd) || !set_nonblocking(fd))
    {
        ::close(fd);
        return nullptr;
    }

    // 已经持有文件锁，同名 socket 只可能是上次异常终止留下的残留。
    if (::unlink(target.c_str()) != 0 && errno != ENOENT)
    {
        ::close(fd);
        return nullptr;
    }

    if (::bind(fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
    {
        ::close(fd);
        return nullptr;
    }

    if (::chmod(target.c_str(), 0600) != 0)
    {
        ::close(fd);
        (void)::unlink(target.c_str());
        return nullptr;
    }

    if (::listen(fd, doorbell_backlog) != 0)
    {
        ::close(fd);
        (void)::unlink(target.c_str());
        return nullptr;
    }

    auto* doorbell = new (std::nothrow) Doorbell();
    if (doorbell == nullptr)
    {
        ::close(fd);
        (void)::unlink(target.c_str());
        return nullptr;
    }
    doorbell->fd = fd;
    std::memcpy(doorbell->path, target.c_str(), target.size() + 1);
    return doorbell;
}

void stop_doorbell(Doorbell* doorbell)
{
    if (doorbell == nullptr)
    {
        return;
    }
    if (doorbell->fd >= 0)
    {
        (void)::close(doorbell->fd);
        doorbell->fd = -1;
    }
    if (doorbell->path[0] != '\0')
    {
        (void)::unlink(doorbell->path);
    }
    delete doorbell;
}

bool wait_doorbell(Doorbell* doorbell, int timeout_ms)
{
    if (doorbell == nullptr || doorbell->fd < 0)
    {
        return false;
    }

    pollfd descriptor;
    descriptor.fd = doorbell->fd;
    descriptor.events = POLLIN;
    descriptor.revents = 0;
    int result = 0;
    do
    {
        result = ::poll(&descriptor, 1, timeout_ms);
    } while (result < 0 && errno == EINTR);
    if (result <= 0)
    {
        return false;
    }

    bool notified = false;
    for (;;)
    {
        const int client = ::accept(doorbell->fd, nullptr, nullptr);
        if (client < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        // 连接本身即为通知；没有 payload，接受后立即关闭。
        (void)::close(client);
        notified = true;
    }
    return notified;
}

void notify_doorbell(const std::string& target)
{
    sockaddr_un address;
    if (!make_unix_address(target, address))
    {
        return;
    }

    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        return;
    }
    if (!set_close_on_exec(fd) || !set_nonblocking(fd))
    {
        ::close(fd);
        return;
    }

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

    // 连接本身已经表示“二次启动发生了”；标记字节只用于完成一次发送，
    // 主实例不读取它。失败时静默忽略，通知是 best-effort。
    const char marker = 1;
    int send_flags = 0;
#ifdef MSG_NOSIGNAL
    send_flags |= MSG_NOSIGNAL;
#endif
    (void)::send(fd, &marker, 1, send_flags);
    ::close(fd);
}

int doorbell_native_fd(const Doorbell* doorbell)
{
    return doorbell == nullptr ? -1 : doorbell->fd;
}

void* doorbell_native_handle(const Doorbell* doorbell)
{
    // POSIX 使用 doorbell_fd()；没有 Windows 那样的可等待句柄。
    (void)doorbell;
    return nullptr;
}

} // namespace detail
} // namespace singleproc

#endif // !defined(_WIN32)
