// 本文件只在 Windows 构建中编译：CMake 按平台二选一，Linux 构建的
// compile_commands.json 里没有本文件，因此 Linux 门禁的 clang-tidy / ASan /
// Valgrind 都不覆盖它。整文件保留 #ifdef _WIN32 只是为了让 Linux 门禁里的
// standardize（其内部会跑 clang-tidy）把本文件当作空翻译单元，避免找不到
// windows.h 的误报；Windows 分支的真实静态检查依赖 Windows 侧构建或 mingw
// 交叉编译。
#ifdef _WIN32

/// @file
/// @brief guard 的 Windows 平台实现：LockFileEx 文件锁与命名管道门铃。
///
/// 语义与 POSIX 实现保持一致：锁文件长期存在，锁由句柄持有，进程退出或
/// 异常终止后由内核释放。门铃使用单实例命名管道，连接本身即为通知。

#include "platform.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include <windows.h>
#include <sddl.h>

namespace singleproc
{
namespace detail
{

namespace
{

constexpr int doorbell_connect_timeout_ms = 100; // 二次实例等待主实例门铃的上限
constexpr DWORD doorbell_buffer_size
    = 64; // 连接即通知；Windows 客户端不写入任何数据，这里只是合法参数
constexpr std::size_t max_wide_path = 260; // MAX_PATH，含结尾 NUL 前的上限
constexpr std::size_t max_pipe_name = 255; // 命名管道名的保守上限

/// @brief 命名管道名的固定前缀（目标字符串统一用 UTF-8 保存）。
constexpr char pipe_prefix[] = R"(\\.\pipe\)";

#ifndef PIPE_REJECT_REMOTE_CLIENTS
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008
#endif

/// @brief 获取 C++11 std::basic_string 的可写缓冲区指针。
///
/// C++11 的 data() 返回 const 指针，写入 Buffer 只能用 &s[0]；集中在这里，
/// 避免每个调用点重复抑制同一条检查。
wchar_t*
writable_data(std::wstring& text)
{
  // NOLINTNEXTLINE(readability-container-data-pointer): C++11 的 data() 返回
  // const 指针
  return &text[0];
}

char*
writable_data(std::string& text)
{
  // NOLINTNEXTLINE(readability-container-data-pointer): C++11 的 data() 返回
  // const 指针
  return &text[0];
}

/// 把 UTF-8 字符串转换为 UTF-16；非法序列返回 false。
bool
utf8_to_wide(const std::string& value, std::wstring& wide)
{
  wide.clear();
  if (value.empty())
    {
      return true;
    }
  const int length = ::MultiByteToWideChar(CP_UTF8,
                                           MB_ERR_INVALID_CHARS,
                                           value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr,
                                           0);
  if (length <= 0)
    {
      return false;
    }
  std::wstring buffer(static_cast<std::size_t>(length), L'\0');
  if (::MultiByteToWideChar(CP_UTF8,
                            MB_ERR_INVALID_CHARS,
                            value.data(),
                            static_cast<int>(value.size()),
                            writable_data(buffer),
                            length)
      != length)
    {
      return false;
    }
  wide.swap(buffer);
  return true;
}

/// 把 UTF-16 字符串转换为 UTF-8；失败时返回空串。
std::string
wide_to_utf8(const std::wstring& wide)
{
  if (wide.empty())
    {
      return {};
    }
  const int length = ::WideCharToMultiByte(CP_UTF8,
                                           0,
                                           wide.data(),
                                           static_cast<int>(wide.size()),
                                           nullptr,
                                           0,
                                           nullptr,
                                           nullptr);
  if (length <= 0)
    {
      return {};
    }
  std::string buffer(static_cast<std::size_t>(length), '\0');
  if (::WideCharToMultiByte(CP_UTF8,
                            0,
                            wide.data(),
                            static_cast<int>(wide.size()),
                            writable_data(buffer),
                            length,
                            nullptr,
                            nullptr)
      != length)
    {
      return {};
    }
  return buffer;
}

/// 读取环境变量；未设置或读取失败时返回空串。
std::string
environment_variable(const wchar_t* name)
{
  const DWORD length = ::GetEnvironmentVariableW(name, nullptr, 0);
  if (length == 0)
    {
      return {};
    }
  std::wstring value(static_cast<std::size_t>(length), L'\0');
  const DWORD written
      = ::GetEnvironmentVariableW(name, writable_data(value), length);
  if (written == 0 || written >= length)
    {
      return {};
    }
  value.resize(static_cast<std::size_t>(written));
  return wide_to_utf8(value);
}

/// @brief 局部句柄守卫：析构时关闭句柄，异常路径也不泄漏。
class Handleguard
{
public:
  /// @brief 接管句柄。
  explicit Handleguard(HANDLE handle) : handle_(handle)
  {
  }

  /// @brief 关闭托管的句柄。
  ~Handleguard()
  {
    if (handle_ != nullptr)
      {
        (void)::CloseHandle(handle_);
      }
  }

  Handleguard(const Handleguard&) = delete;
  Handleguard& operator=(const Handleguard&) = delete;

private:
  /// @brief 托管的句柄。
  HANDLE handle_ = nullptr;
};

/// @brief 局部 LocalAlloc 内存守卫：析构时释放，异常路径也不泄漏。
class LocalMemoryguard
{
public:
  /// @brief 接管内存块。
  explicit LocalMemoryguard(HLOCAL memory) : memory_(memory)
  {
  }

  /// @brief 释放托管的内存块。
  ~LocalMemoryguard()
  {
    if (memory_ != nullptr)
      {
        (void)::LocalFree(memory_);
      }
  }

  LocalMemoryguard(const LocalMemoryguard&) = delete;
  LocalMemoryguard& operator=(const LocalMemoryguard&) = delete;

private:
  /// @brief 托管的内存块。
  HLOCAL memory_ = nullptr;
};

/// 把 GetLastError() 映射成与 POSIX 分支一致的 errno。
int
map_win32_error(DWORD error)
{
  switch (error)
    {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      return ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
      return EACCES;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_DRIVE:
    case ERROR_DIRECTORY:
      return EINVAL;
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_BUFFER_OVERFLOW:
      return ENAMETOOLONG;
    case ERROR_TOO_MANY_OPEN_FILES:
      return EMFILE;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
      return ENOMEM;
    case ERROR_WRITE_PROTECT:
      return EROFS;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
      return ENOSPC;
    default:
      return EIO;
    }
}

/// 去掉最后一级后的父路径；没有父路径时返回空串。
std::wstring
parent_path(const std::wstring& path)
{
  std::size_t end = path.size();
  while (end > 0 && (path[end - 1] == L'\\' || path[end - 1] == L'/'))
    {
      --end;
    }
  if (end == 0)
    {
      return {};
    }
  const std::size_t separator = path.find_last_of(L"\\/", end - 1);
  if (separator == std::wstring::npos)
    {
      return {};
    }
  std::wstring parent = path.substr(0, separator);
  if (parent.size() == 2 && parent[1] == L':')
    {
      parent += L'\\';
    }
  return parent;
}

/// 路径错误映射：区分“目录不存在”和“路径中的某一级不是目录”。
///
/// POSIX 打开路径时由内核给出 ENOENT / ENOTDIR；Windows 的 CreateFileW
/// 对两种情况都返回 ERROR_PATH_NOT_FOUND，且可能只在更深的某一级不是目录。
/// 这里从最近一级父路径往上找第一个存在的祖先：它是文件就给 ENOTDIR，
/// 找不到任何存在祖先才给 ENOENT，从而与 POSIX 保持一致。
int
map_path_error(const std::wstring& path, DWORD error)
{
  if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
    {
      return map_win32_error(error);
    }

  std::wstring parent = parent_path(path);
  while (!parent.empty())
    {
      const DWORD attributes = ::GetFileAttributesW(parent.c_str());
      if (attributes != INVALID_FILE_ATTRIBUTES)
        {
          return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? ENOENT
                                                              : ENOTDIR;
        }
      parent = parent_path(parent);
    }
  return ENOENT;
}

/// 当前用户的 SID 字符串；获取失败时返回空串。
std::string
query_user_sid_string()
{
  HANDLE token = nullptr;
  // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
  // 平台惯例直接作条件，显式比较只增噪音
  if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token))
    {
      return {};
    }
  const Handleguard token_guard(token);

  DWORD size = 0;
  (void)::GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::string result;
  if (size >= sizeof(TOKEN_USER))
    {
      std::vector<unsigned char> buffer(size);
      // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
      // 平台惯例直接作条件，显式比较只增噪音
      if (::GetTokenInformation(token, TokenUser, buffer.data(), size, &size))
        {
          // TOKEN_USER 里有指针成员，按值拷贝到对齐的局部变量，避免对字节缓冲区
          // 直接做可能未对齐的 reinterpret_cast。
          TOKEN_USER token_user;
          std::memcpy(&token_user, buffer.data(), sizeof(token_user));
          LPWSTR sid_text = nullptr;
          // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32
          // BOOL(int) 平台惯例直接作条件，显式比较只增噪音
          if (::ConvertSidToStringSidW(token_user.User.Sid, &sid_text))
            {
              const LocalMemoryguard sid_guard(sid_text);
              result = wide_to_utf8(sid_text);
            }
        }
    }
  return result;
}

/// 生成用户标识：优先 SID，失败时退回清洗过的用户名。
std::string
load_user_identifier()
{
  std::string identity = query_user_sid_string();
  if (identity.empty())
    {
      identity = environment_variable(L"USERNAME");
      for (char& value : identity)
        {
          const bool alpha = (value >= 'a' && value <= 'z')
                             || (value >= 'A' && value <= 'Z');
          const bool digit = value >= '0' && value <= '9';
          if (!alpha && !digit && value != '.' && value != '_' && value != '-')
            {
              value = '_';
            }
        }
    }
  if (identity.empty())
    {
      identity = "unknown";
    }
  return identity;
}

/// 默认运行时目录：%LOCALAPPDATA% → %TEMP% → GetTempPathW。
std::string
load_default_runtime_dir()
{
  std::string directory = environment_variable(L"LOCALAPPDATA");
  if (!directory.empty())
    {
      return directory;
    }
  directory = environment_variable(L"TEMP");
  if (!directory.empty())
    {
      return directory;
    }

  wchar_t buffer[MAX_PATH + 1];
  const DWORD length = ::GetTempPathW(MAX_PATH + 1, buffer);
  if (length == 0)
    {
      return {};
    }
  std::wstring path(buffer, static_cast<std::size_t>(length));
  while (!path.empty()
         && (path[path.size() - 1] == L'\\' || path[path.size() - 1] == L'/'))
    {
      path.resize(path.size() - 1);
    }
  return wide_to_utf8(path);
}

/// 对已存在的路径做类型预检，不跟随符号链接。
///
/// 目录需要 FILE_FLAG_BACKUP_SEMANTICS 才能用只读属性权限打开；
/// FILE_READ_ATTRIBUTES 足够做类型判断。路径不存在或不可访问时返回 0，
/// 由后续正式打开产生并映射真实错误。
///
/// @return 0 表示可以继续打开；否则返回对应的 errno（ELOOP / EISDIR）。
int
classify_existing_path(const std::wstring& path)
{
  const HANDLE handle
      = ::CreateFileW(path.c_str(),
                      FILE_READ_ATTRIBUTES,
                      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                      nullptr,
                      OPEN_EXISTING,
                      FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                      nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    {
      return 0;
    }

  BY_HANDLE_FILE_INFORMATION information;
  std::memset(&information, 0, sizeof(information));
  int result = 0;
  // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
  // 平台惯例直接作条件，显式比较只增噪音
  if (::GetFileInformationByHandle(handle, &information))
    {
      if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
          result = ELOOP;
        }
      else if ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
        {
          result = EISDIR;
        }
    }
  (void)::CloseHandle(handle);
  return result;
}

/// 校验已打开的锁文件：必须是磁盘上的普通文件，且不是重解析点。
///
/// @return 0 表示通过；否则返回对应的 errno。
int
validate_lock_file(HANDLE handle)
{
  if (::GetFileType(handle) != FILE_TYPE_DISK)
    {
      return EINVAL;
    }

  BY_HANDLE_FILE_INFORMATION information;
  std::memset(&information, 0, sizeof(information));
  // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
  // 平台惯例直接作条件，显式比较只增噪音
  if (!::GetFileInformationByHandle(handle, &information))
    {
      return map_win32_error(::GetLastError());
    }
  if ((information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
      return ELOOP;
    }
  if ((information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
      return EISDIR;
    }
  return 0;
}

/// 构造只允许指定用户 SID 访问的安全描述符；SDDL 不可用时退回默认安全描述符。
///
/// DACL 只有一条 ACE，授予当前用户完全访问权，对应 POSIX 上 socket 的 0600；
/// 不使用 OWNER RIGHTS（OW），因为管理员进程的对象所有者可能是 Administrators
/// 组，OW 会让同一用户的普通进程连不上。
SECURITY_ATTRIBUTES*
make_user_only_security(const std::string& sid_string,
                        SECURITY_ATTRIBUTES& storage,
                        PSECURITY_DESCRIPTOR& descriptor)
{
  std::wstring wide_sid;
  if (sid_string.empty() || !utf8_to_wide(sid_string, wide_sid))
    {
      return nullptr;
    }
  const std::wstring sddl = L"D:P(A;;GA;;;" + wide_sid + L")";
  // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
  // 平台惯例直接作条件，显式比较只增噪音
  if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
          sddl.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
    {
      descriptor = nullptr;
      return nullptr;
    }
  storage.nLength = sizeof(storage);
  storage.lpSecurityDescriptor = descriptor;
  storage.bInheritHandle = FALSE;
  return &storage;
}

/// 投递一次重叠式 ConnectNamedPipe；同步完成时记下“已连接”。
bool arm_connect(Doorbell& doorbell);

} // namespace

/// @brief 门铃监听端：单实例命名管道 + 连接完成事件。
///
/// 单实例是刻意的：Windows 上没有 fd，对外只暴露一个可等待事件，
/// 事件在任意一次连接完成时变为有信号。多个二次实例同时到达时由
/// WaitNamedPipe 排队合并，符合“允许合并为一次回调”的既有语义。
struct Doorbell
{
  /// @brief 管道服务端句柄。
  HANDLE pipe = nullptr;

  /// @brief 连接完成/就绪事件，也就是 doorbell_handle() 返回的句柄。
  HANDLE event = nullptr;

  /// @brief ConnectNamedPipe 使用的重叠结构。
  OVERLAPPED overlapped = {};

  /// @brief 是否存在已建立、尚未处理的连接。
  bool connected = false;

  /// @brief 是否收到过“连上又立刻断开”的通知（ERROR_NO_DATA 路径）。
  ///
  /// 与 connected 分开：这种连接已经消失，只需要把“发生过”交给上层。
  bool notification = false;
};

namespace
{

bool
arm_connect(Doorbell& doorbell)
{
  // ERROR_NO_DATA 表示客户端在 ConnectNamedPipe 之前连上、又已经关闭；
  // 这是“连接发生过”的通知，不能当成失败，否则门铃会被永久禁用。
  // 清理旧连接状态后重新投递监听；客户端持续抢连时给少量重试上限，避免活锁。
  constexpr int max_attempts = 4;
  for (int attempt = 0; attempt < max_attempts; ++attempt)
    {
      std::memset(&doorbell.overlapped, 0, sizeof(doorbell.overlapped));
      doorbell.overlapped.hEvent = doorbell.event;
      (void)::ResetEvent(doorbell.event);

      // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
      // 平台惯例直接作条件，显式比较只增噪音
      if (::ConnectNamedPipe(doorbell.pipe, &doorbell.overlapped))
        {
          doorbell.connected = true;
          (void)::SetEvent(doorbell.event);
          return true;
        }
      const DWORD error = ::GetLastError();
      if (error == ERROR_IO_PENDING)
        {
          return true;
        }
      if (error == ERROR_PIPE_CONNECTED)
        {
          // 客户端在 ConnectNamedPipe 之前就连上了，Windows 用这个错误码通知。
          doorbell.connected = true;
          (void)::SetEvent(doorbell.event);
          return true;
        }
      if (error == ERROR_NO_DATA)
        {
          // 连接来过又断开：先记下通知，清掉旧连接状态，再重新挂起监听。
          doorbell.notification = true;
          (void)::DisconnectNamedPipe(doorbell.pipe);
          continue;
        }
      return false;
    }
  return false;
}

/// 关闭监听端；门铃从此不可用，但不影响已经取得的文件锁。
void
abandon_doorbell(Doorbell& doorbell)
{
  if (doorbell.pipe != nullptr)
    {
      // CancelIoEx 只“请求”取消，不等待操作完成；Windows 契约要求应用在
      // 取消后、关闭句柄或复用 OVERLAPPED 之前等该 IO 真正结束，否则内核
      // 可能在之后写已经释放的内存。CancelIoEx 返回 FALSE 且
      // ERROR_NOT_FOUND 表示没有未完成的 IO，无需等待。
      // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
      // 平台惯例直接作条件，显式比较只增噪音
      if (::CancelIoEx(doorbell.pipe, &doorbell.overlapped))
        {
          DWORD transferred = 0;
          (void)::GetOverlappedResult(
              doorbell.pipe, &doorbell.overlapped, &transferred, TRUE);
        }
      (void)::CloseHandle(doorbell.pipe);
      doorbell.pipe = nullptr;
    }
  doorbell.connected = false;
  doorbell.notification = false;
}

} // namespace

char
path_separator()
{
  return '\\';
}

std::string
default_runtime_dir()
{
  return load_default_runtime_dir();
}

std::string
user_identifier()
{
  return load_user_identifier();
}

lock_attempt
acquire_lock(const std::string& path)
{
  lock_attempt attempt;

  std::wstring wide_path;
  if (!utf8_to_wide(path, wide_path))
    {
      attempt.error_code = EINVAL;
      return attempt;
    }
  if (wide_path.size() >= max_wide_path)
    {
      attempt.error_code = ENAMETOOLONG;
      return attempt;
    }

  // 已存在路径先做类型预检：目录给 EISDIR、符号链接给 ELOOP，与 POSIX 的
  // open(O_NOFOLLOW) 错误码对齐。路径不存在时继续走创建流程。
  const int existing = classify_existing_path(wide_path);
  if (existing != 0)
    {
      attempt.error_code = existing;
      return attempt;
    }

  // FILE_FLAG_OPEN_REPARSE_POINT：不跟随符号链接，与 POSIX 的 O_NOFOLLOW 对齐。
  const HANDLE handle
      = ::CreateFileW(wide_path.c_str(),
                      GENERIC_READ | GENERIC_WRITE,
                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                      nullptr,
                      OPEN_ALWAYS,
                      FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
                      nullptr);
  if (handle == INVALID_HANDLE_VALUE)
    {
      attempt.error_code = map_path_error(wide_path, ::GetLastError());
      return attempt;
    }

  const int validation = validate_lock_file(handle);
  if (validation != 0)
    {
      (void)::CloseHandle(handle);
      attempt.error_code = validation;
      return attempt;
    }

  OVERLAPPED overlapped;
  std::memset(&overlapped, 0, sizeof(overlapped));
  // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
  // 平台惯例直接作条件，显式比较只增噪音
  if (!::LockFileEx(handle,
                    LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY,
                    0,
                    MAXDWORD,
                    MAXDWORD,
                    &overlapped))
    {
      const DWORD error = ::GetLastError();
      (void)::CloseHandle(handle);
      if (error == ERROR_LOCK_VIOLATION)
        {
          // 已持锁：同一进程的第二个句柄同样会拿到这个错误码。
          attempt.result = lock_result::already_running;
          attempt.error_code = 0;
          return attempt;
        }
      attempt.error_code = map_win32_error(error);
      return attempt;
    }

  attempt.result = lock_result::primary;
  attempt.handle = reinterpret_cast<lock_handle>(handle);
  return attempt;
}

void
release_lock(lock_handle handle)
{
  if (handle == invalid_lock_handle)
    {
      return;
    }
  // NOLINTNEXTLINE(performance-no-int-to-ptr): lock_handle 用 intptr_t 承载
  // fd/HANDLE，避免平台类型进入跨平台接口
  auto* const file = reinterpret_cast<HANDLE>(handle);
  // MS 文档建议在关闭前显式解锁：只依赖 CloseHandle 时，系统释放锁的时机
  // 可能滞后（资源紧张时尤其明显）。这里忽略失败：句柄关闭仍会兜底释放。
  OVERLAPPED overlapped;
  std::memset(&overlapped, 0, sizeof(overlapped));
  (void)::UnlockFileEx(file, 0, MAXDWORD, MAXDWORD, &overlapped);
  (void)::CloseHandle(file);
}

std::string
make_doorbell_target(const std::string& runtime_dir,
                     const std::string& runtime_name)
{
  // Windows 的命名管道没有目录概念，runtime_dir 只在 POSIX 分支使用。
  (void)runtime_dir;
  return std::string(pipe_prefix) + runtime_name + ".doorbell";
}

bool
valid_doorbell_target(const std::string& target)
{
  std::wstring wide_name;
  if (!utf8_to_wide(target, wide_name))
    {
      return false;
    }
  return !wide_name.empty() && wide_name.size() <= max_pipe_name;
}

Doorbell*
start_doorbell(const std::string& target)
{
  std::wstring wide_name;
  if (!utf8_to_wide(target, wide_name) || wide_name.size() > max_pipe_name)
    {
      return nullptr;
    }

  SECURITY_ATTRIBUTES security_storage;
  std::memset(&security_storage, 0, sizeof(security_storage));
  PSECURITY_DESCRIPTOR security_descriptor = nullptr;
  SECURITY_ATTRIBUTES* security = make_user_only_security(
      query_user_sid_string(), security_storage, security_descriptor);

  auto* doorbell = new (std::nothrow) Doorbell();
  if (doorbell == nullptr)
    {
      if (security_descriptor != nullptr)
        {
          (void)::LocalFree(security_descriptor);
        }
      return nullptr;
    }

  doorbell->pipe
      = ::CreateNamedPipeW(wide_name.c_str(),
                           PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT
                               | PIPE_REJECT_REMOTE_CLIENTS,
                           1,
                           doorbell_buffer_size,
                           doorbell_buffer_size,
                           0,
                           security);
  if (security_descriptor != nullptr)
    {
      (void)::LocalFree(security_descriptor);
    }
  if (doorbell->pipe == INVALID_HANDLE_VALUE)
    {
      doorbell->pipe = nullptr;
      delete doorbell;
      return nullptr;
    }

  doorbell->event = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (doorbell->event == nullptr)
    {
      (void)::CloseHandle(doorbell->pipe);
      delete doorbell;
      return nullptr;
    }

  if (!arm_connect(*doorbell))
    {
      (void)::CloseHandle(doorbell->event);
      (void)::CloseHandle(doorbell->pipe);
      delete doorbell;
      return nullptr;
    }
  return doorbell;
}

void
stop_doorbell(Doorbell* doorbell)
{
  if (doorbell == nullptr)
    {
      return;
    }
  abandon_doorbell(*doorbell);
  if (doorbell->event != nullptr)
    {
      (void)::CloseHandle(doorbell->event);
    }
  delete doorbell;
}

bool
wait_doorbell(Doorbell* doorbell, int timeout_ms)
{
  if (doorbell == nullptr || doorbell->pipe == nullptr)
    {
      return false;
    }

  DWORD timeout = timeout_ms < 0 ? INFINITE : static_cast<DWORD>(timeout_ms);
  bool notified = false;
  for (;;)
    {
      if (doorbell->notification)
        {
          // 先交付“连上又立刻断开”的通知，再继续等待/排空。
          doorbell->notification = false;
          notified = true;
          timeout = 0;
          continue;
        }
      if (!doorbell->connected)
        {
          const DWORD state = ::WaitForSingleObject(doorbell->event, timeout);
          if (state == WAIT_TIMEOUT)
            {
              break;
            }
          if (state != WAIT_OBJECT_0)
            {
              break;
            }
          DWORD transferred = 0;
          // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32
          // BOOL(int) 平台惯例直接作条件，显式比较只增噪音
          if (!::GetOverlappedResult(
                  doorbell->pipe, &doorbell->overlapped, &transferred, FALSE))
            {
              // 无法解释的等待结果：让门铃失效，避免事件一直保持 signaled
              // 把事件驱动调用方带进忙循环。
              abandon_doorbell(*doorbell);
              return notified;
            }
        }

      // 连接本身即为通知；断开后立刻重新挂起监听，准备接收下一次通知。
      doorbell->connected = false;
      (void)::DisconnectNamedPipe(doorbell->pipe);
      if (!arm_connect(*doorbell))
        {
          abandon_doorbell(*doorbell);
          return notified;
        }
      notified = true;
      timeout = 0; // 先排空已经排队的连接，再回到等待
    }
  return notified;
}

void
notify_doorbell(const std::string& target)
{
  std::wstring wide_name;
  if (!utf8_to_wide(target, wide_name))
    {
      return;
    }

  const ULONGLONG deadline
      = ::GetTickCount64()
        + static_cast<ULONGLONG>(doorbell_connect_timeout_ms);
  for (;;)
    {
      // 连接即通知，不发送 payload；打开管道就表示“二次启动发生了”。
      const HANDLE client = ::CreateFileW(wide_name.c_str(),
                                          GENERIC_WRITE,
                                          0,
                                          nullptr,
                                          OPEN_EXISTING,
                                          0,
                                          nullptr);
      if (client != INVALID_HANDLE_VALUE)
        {
          (void)::CloseHandle(client);
          return;
        }

      if (::GetLastError() != ERROR_PIPE_BUSY)
        {
          // 主实例没有监听或拒绝连接：通知是 best-effort，直接放弃。
          return;
        }

      const ULONGLONG now = ::GetTickCount64();
      if (now >= deadline)
        {
          return;
        }
      // NOLINTNEXTLINE(readability-implicit-bool-conversion): Win32 BOOL(int)
      // 平台惯例直接作条件，显式比较只增噪音
      if (!::WaitNamedPipeW(wide_name.c_str(),
                            static_cast<DWORD>(deadline - now)))
        {
          return;
        }
    }
}

int
doorbell_native_fd(const Doorbell* doorbell)
{
  // Windows 没有 fd 概念；对公共 API 保持恒为 -1。
  (void)doorbell;
  return -1;
}

void*
doorbell_native_handle(const Doorbell* doorbell)
{
  return doorbell == nullptr ? nullptr
                             : reinterpret_cast<void*>(doorbell->event);
}

} // namespace detail
} // namespace singleproc

#endif // _WIN32
