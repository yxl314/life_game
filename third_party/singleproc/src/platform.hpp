///
/// \file
/// \brief                      guard 的平台实现接口，只在库内部使用。
///
/// 公开头文件 include/singleproc/singleproc.hpp 不引用本文件；平台差异
/// （锁文件、门铃、路径、errno 映射）全部收敛到 platform_posix.cpp 与
/// platform_win32.cpp 两份实现里，业务逻辑只调用这里的函数。
///

#ifndef SINGLEPROC_PLATFORM_HPP
#define SINGLEPROC_PLATFORM_HPP

#include <cstdint>
#include <string>

namespace singleproc
{
namespace detail
{

///
/// \brief                      平台路径分隔符。
///
/// POSIX 为 '/'，Windows 为 '\'。只用于拼接库内部的运行时路径。
///
char path_separator();

///
/// \brief                      默认运行时目录。
///
/// POSIX 优先 `$XDG_RUNTIME_DIR`，未设置时退回 `/tmp`；
/// Windows 优先 `%LOCALAPPDATA%`，未设置时退回 `%TEMP%`。
///
/// \return                     默认目录；平台兜底失败时返回空串，由调用方按错误处理。
///
std::string default_runtime_dir();

///
/// \brief                      当前用户标识，用于拼接锁文件名与门铃名。
///
/// POSIX 返回十进制 UID；Windows 返回用户 SID 字符串。
///
/// \return                     用户标识；获取失败时返回可安全用于文件名的退化值。
///
std::string user_identifier();

///
/// \brief                      拼接目录与文件名，目录末尾缺少分隔符时自动补齐。
///
/// 只认平台自己的分隔符：POSIX 上 '\\' 是合法的文件名字符，不能当分隔符，
/// 否则锁文件会落到与旧版本不同的路径（混合版本会双开）。Windows 上正斜杠
/// 也会被系统接受，这里补 '\\' 后同样可用。
///
/// \param directory            目录；为空时返回 name 本身。
/// \param name                 文件名或子路径。
/// \return                     拼接后的路径。
///
inline std::string
join_path(const std::string& directory, const std::string& name)
{
  if (directory.empty())
    {
      return name;
    }
  std::string path = directory;
  const char separator = path_separator();
  if (path[path.size() - 1] != separator)
    {
      path += separator;
    }
  return path + name;
}

///
/// \brief                      锁句柄：POSIX 为文件描述符，Windows 为 HANDLE 的位模式。
///
using lock_handle = std::intptr_t;

///
/// \brief                      无效锁句柄。
///
constexpr lock_handle invalid_lock_handle = -1;

///
/// \brief                      独占锁的尝试结果。
///
enum class lock_result
{
  ///
  /// \brief                    取得独占锁。
  ///
  primary,

  ///
  /// \brief                    同一个锁文件已被其它句柄持有。
  ///
  already_running,

  ///
  /// \brief                    打开锁文件或加锁失败。
  ///
  error,
};

///
/// \brief                      acquire_lock() 的返回值。
///
struct lock_attempt
{
public:
  ///
  /// \brief                    尝试结果。
  ///
  lock_result result = lock_result::error;
  ///
  /// \brief                    成功时的锁句柄；其余情况为 invalid_lock_handle。
  ///
  lock_handle handle = invalid_lock_handle;
  ///
  /// \brief                    Error 时的 errno；其余情况为 0。
  ///
  int error_code = 0;
};

///
/// \brief                      创建并尝试获取锁文件的独占锁。
///
/// 调用方负责 key 校验与路径长度校验；平台实现负责打开文件、校验文件
/// 类型（普通文件、非符号链接）并加非阻塞独占锁。
///
/// \param path                 锁文件路径。
/// \return                     锁句柄、AlreadyRunning 或带 errno 的失败。
///
lock_attempt acquire_lock(const std::string& path);

///
/// \brief                      释放锁句柄。
///
/// 关闭句柄后锁由系统释放；不删除锁文件。
///
/// \param handle               由 acquire_lock() 返回的句柄。
///
void release_lock(lock_handle handle);

///
/// \brief                      门铃监听端，由各平台实现定义。
///
struct Doorbell;

///
/// \brief                      由运行时目录与运行时文件名生成门铃目标。
///
/// POSIX 是 Unix domain socket 路径；Windows 是命名管道名，两者都只由
/// 字母、数字和 `-` 组成，不含目录信息。
///
/// \param runtime_dir          运行时目录；Windows 实现忽略该参数。
/// \param runtime_name         锁文件与门铃共用的运行时文件名。
/// \return                     门铃目标字符串。
///
std::string make_doorbell_target(const std::string& runtime_dir,
                                 const std::string& runtime_name);

///
/// \brief                      判断门铃目标是否在平台长度限制之内。
///
/// 超出限制时调用方只禁用门铃，独占锁仍然有效。
///
/// \param target               门铃目标字符串。
/// \return                     可用时返回 true。
///
bool valid_doorbell_target(const std::string& target);

///
/// \brief                      创建并开始监听门铃。
///
/// 失败时返回 nullptr，调用方只禁用门铃，不影响单实例锁。
///
/// \param target               由 make_doorbell_target() 生成的目标。
/// \return                     监听端；失败为 nullptr。
///
Doorbell* start_doorbell(const std::string& target);

///
/// \brief                      停止监听并释放门铃资源。
///
/// \param doorbell             监听端；允许为 nullptr。
///
void stop_doorbell(Doorbell* doorbell);

///
/// \brief                      等待并收集二次启动通知，多个连接会被合并。
///
/// \param doorbell             监听端。
/// \param timeout_ms           最长等待毫秒数；0 表示不等待，负数表示无限等待。
/// \return                     true 表示至少收到一个通知。
///
bool wait_doorbell(Doorbell* doorbell, int timeout_ms);

///
/// \brief                      二次实例：尽力向主实例发送一次通知。
///
/// 通知是 best-effort：主实例未监听、连接超时或发送失败时静默放弃。
///
/// \param target               由 make_doorbell_target() 生成的目标。
///
void notify_doorbell(const std::string& target);

///
/// \brief                      门铃监听端的原生 fd。
///
/// \param doorbell             监听端。
/// \return                     POSIX 为监听 fd；Windows 恒为 -1。
///
int doorbell_native_fd(const Doorbell* doorbell);

///
/// \brief                      门铃监听端的原生可等待句柄。
///
/// \param doorbell             监听端。
/// \return                     Windows 为通知就绪事件 HANDLE；POSIX 恒为 nullptr。
///
void* doorbell_native_handle(const Doorbell* doorbell);

} // namespace detail
} // namespace singleproc

#endif // SINGLEPROC_PLATFORM_HPP
