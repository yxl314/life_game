#include <singleproc/singleproc.hpp>

#include <cerrno>
#include <cstdlib>
#include <string>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

namespace
{

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

/// 生成锁文件路径，并在超过常见路径上限时返回空串。
std::string lock_path(const std::string& directory, const std::string& key)
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
    const std::string path = base + "singleproc-" +
                             std::to_string(static_cast<unsigned long>(::getuid())) +
                             "-" + key + ".lock";
    if (path.size() >= 4096)
    {
        return std::string();
    }
    return path;
}

} // namespace

namespace singleproc
{

Guard::Guard(const std::string& key, const std::string& runtime_dir)
    : fd_(-1), state_(State::Error), error_code_(0)
{
    if (!valid_key(key))
    {
        error_code_ = EINVAL;
        return;
    }

    const std::string path = lock_path(runtime_dir, key);
    if (path.empty())
    {
        error_code_ = ENAMETOOLONG;
        return;
    }

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
        error_code_ = errno;
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
}

Guard::~Guard()
{
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

bool Guard::isPrimary() const noexcept
{
    return state_ == State::Primary;
}

int Guard::errorCode() const noexcept
{
    return error_code_;
}

} // namespace singleproc
