/// @file
/// @brief Guard 的平台无关业务逻辑：锁文件命名、状态机与门铃分发。
///
/// 平台差异全部收敛在 platform.hpp 与 platform_*.cpp 中，本文件不出现
/// 平台宏分支。

#include <singleproc/singleproc.hpp>

#include "platform.hpp"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>

namespace
{

/// 路径长度上限：POSIX 取 PATH_MAX；Windows 在平台实现里还有更严格的限制。
constexpr std::size_t max_path_length = 4096;

/// 判断 key 是否能安全地作为文件名的一部分。
bool valid_key(const std::string& key)
{
    if (key.empty())
    {
        return false;
    }
    return std::all_of(key.begin(),
                       key.end(),
                       [](char value)
                       {
                           const bool alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
                           const bool digit = value >= '0' && value <= '9';
                           return alpha || digit || value == '.' || value == '_' || value == '-';
                       });
}

/// 目录字符串中不能嵌入 NUL，否则路径会被系统调用静默截断。
bool valid_directory(const std::string& directory)
{
    return directory.find('\0') == std::string::npos;
}

} // namespace

namespace singleproc
{

struct Guard::Impl
{
    /// @brief 锁句柄：POSIX 为 fd，Windows 为 HANDLE；无效值由平台层定义。
    detail::LockHandle lock_handle = detail::invalid_lock_handle;

    /// @brief 门铃监听端；nullptr 表示门铃未启用。
    detail::Doorbell* doorbell = nullptr;

    /// @brief 启动判定的结果。
    State state = State::Error;

    /// @brief Error 状态下的 errno；其余状态为 0。
    int error_code = 0;

    /// @brief 已注册的二次启动回调。
    DoorbellHandler doorbell_handler;

    /// @brief 已接入连接但尚未分发的通知。
    bool doorbell_pending = false;
};

Guard::Guard(const std::string& key, const std::string& runtime_dir): impl_(new Impl())
{
    if (!valid_key(key) || !valid_directory(runtime_dir))
    {
        impl_->error_code = EINVAL;
        return;
    }

    const std::string base = runtime_dir.empty() ? detail::default_runtime_dir() : runtime_dir;
    if (base.empty())
    {
        impl_->error_code = ENOENT;
        return;
    }

    // 锁文件与门铃共用同一个运行时文件名：只改一处命名规则时两者不会分家。
    const std::string runtime_name = "singleproc-" + detail::user_identifier() + "-" + key;

    const std::string lock_path = detail::join_path(base, runtime_name + ".lock");
    if (lock_path.size() >= max_path_length)
    {
        impl_->error_code = ENAMETOOLONG;
        return;
    }

    // 门铃目标超出平台限制（POSIX 的 sun_path、Windows 的管道名长度）时
    // 只禁用门铃，单实例锁仍然有效。
    const std::string doorbell_target = detail::make_doorbell_target(base, runtime_name);
    const bool doorbell_possible = detail::valid_doorbell_target(doorbell_target);

    const detail::LockAttempt attempt = detail::acquire_lock(lock_path);
    if (attempt.result == detail::LockResult::Error)
    {
        impl_->error_code = attempt.error_code;
        return;
    }
    if (attempt.result == detail::LockResult::AlreadyRunning)
    {
        impl_->state = State::AlreadyRunning;
        impl_->error_code = 0;
        if (doorbell_possible)
        {
            detail::notify_doorbell(doorbell_target);
        }
        return;
    }

    impl_->lock_handle = attempt.handle;
    impl_->state = State::Primary;
    impl_->error_code = 0;

    if (doorbell_possible)
    {
        // 取锁之后才创建门铃：平台实现里仍可能有内存分配（Windows 的宽字符
        // 转换、SID 查询等）。构造若在这里抛 bad_alloc，析构函数不会执行，
        // 必须先把锁释放掉，再让异常继续传播。
        try
        {
            impl_->doorbell = detail::start_doorbell(doorbell_target);
        }
        catch (...)
        {
            detail::release_lock(impl_->lock_handle);
            impl_->lock_handle = detail::invalid_lock_handle;
            throw;
        }
    }
}

Guard::~Guard()
{
    if (impl_->doorbell != nullptr)
    {
        detail::stop_doorbell(impl_->doorbell);
        impl_->doorbell = nullptr;
    }

    if (impl_->lock_handle != detail::invalid_lock_handle)
    {
        detail::release_lock(impl_->lock_handle);
        impl_->lock_handle = detail::invalid_lock_handle;
    }
}

Guard::State Guard::state() const noexcept
{
    return impl_->state;
}

bool Guard::is_primary() const noexcept
{
    return impl_->state == State::Primary;
}

int Guard::error_code() const noexcept
{
    return impl_->error_code;
}

void Guard::set_doorbell_handler(DoorbellHandler handler)
{
    impl_->doorbell_handler = std::move(handler);
}

bool Guard::process_doorbell(int timeout_ms)
{
    if (impl_->doorbell == nullptr)
    {
        return false;
    }

    if (!impl_->doorbell_pending && !detail::wait_doorbell(impl_->doorbell, timeout_ms))
    {
        return false;
    }
    impl_->doorbell_pending = true;

    // 复制一份再调用：handler 内替换自己时不会销毁正在执行的函数对象。
    const DoorbellHandler handler = impl_->doorbell_handler;
    if (!handler)
    {
        return false;
    }

    impl_->doorbell_pending = false;
    handler();
    return true;
}

int Guard::doorbell_fd() const noexcept
{
    return impl_->doorbell == nullptr ? -1 : detail::doorbell_native_fd(impl_->doorbell);
}

void* Guard::doorbell_handle() const noexcept
{
    return impl_->doorbell == nullptr ? nullptr : detail::doorbell_native_handle(impl_->doorbell);
}

} // namespace singleproc
