#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <QWidget>

#include <singleproc/singleproc.hpp>
#include <stacklog/stacklog.hpp>

#include "app/main_window.h"
#include "view/theme.h"

namespace
{

/// @brief stacklog 文件 sink 的初始化结果。
struct LogSetup
{
    bool ready = false;
    QString path;
};

/**
 * @brief 配置 stacklog 文件 sink。
 * @param verbose 是否启用 Debug 级别。
 * @param requestedPath 可选的显式日志文件路径。
 */
LogSetup setupLogging(bool verbose, const QString& requestedPath)
{
    QString path = requestedPath;
    if (path.isEmpty())
    {
        path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
               QStringLiteral("/logs/life-game.log");
    }

    const QFileInfo info(path);
    const QString directory = info.absolutePath();
    QString baseName = info.completeBaseName();
    if (baseName.isEmpty())
    {
        baseName = QStringLiteral("life-game");
    }
    QDir().mkpath(directory);

    stacklog::config config;
    config.dir = directory.toStdString();
    config.base_name = baseName.toStdString();
    config.max_file_size = 2ULL * 1024 * 1024;
    config.max_files = 3;
    config.min_level = verbose ? stacklog::level::debug : stacklog::level::info;

    LogSetup setup;
    setup.ready = stacklog::init(config);
    setup.path = QDir(directory).filePath(baseName + QStringLiteral(".log"));
    return setup;
}

} // namespace

// 审计用：GUI 程序没有命令行退出路径，无头环境里只能被杀掉——那样拿不到
// 正常退出流程产生的泄漏汇总。用定时器让窗口自己关，Qt 走完整的 close →
// quit → 析构链路。只在显式设置了 CXX_GUI_AUTOCLOSE_MS 时生效，正常启动
// 不受任何影响。
static void installAutoClose(QWidget* window)
{
    const QByteArray raw = qgetenv("CXX_GUI_AUTOCLOSE_MS");
    bool ok = false;
    const int milliseconds = raw.toInt(&ok);
    if (!ok || milliseconds <= 0)
    {
        return;
    }
    QTimer::singleShot(milliseconds, window, [window]() { window->close(); });
}


int main(int argc, char* argv[])
{
    // Qt 6 默认启用高 DPI 缩放和高 DPI 位图，不再需要设置旧属性。
    QApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("life-game"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("life-game.local"));
    QCoreApplication::setApplicationName(QStringLiteral("life-game"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("Conway's Game of Life"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Conway's Game of Life"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption verboseOption(QStringLiteral("verbose"),
                                           QStringLiteral("Enable debug logging."));
    const QCommandLineOption logFileOption(
        QStringLiteral("log-file"), QStringLiteral("Write logs to this path."), QStringLiteral("path"));
    parser.addOption(verboseOption);
    parser.addOption(logFileOption);
    parser.process(application);

    const LogSetup logSetup = setupLogging(parser.isSet(verboseOption), parser.value(logFileOption));

    int exitCode = 0;
    {
        STACKLOG_TRACE("main");

        STACKLOG_INFO() << "starting life-game " << QCoreApplication::applicationVersion().toStdString()
                        << " Qt " << qVersion() << " pid " << QCoreApplication::applicationPid()
                        << " log " << logSetup.path.toStdString();
        if (!logSetup.ready)
        {
            STACKLOG_WARN() << "file logging is unavailable; using stderr only";
        }

        // 本应用暂未接入门铃；重复启动直接退出，不尝试把已有窗口带到前台。
        const singleproc::Guard guard("life-game");
        if (guard.state() == singleproc::Guard::State::Error)
        {
            STACKLOG_ERROR() << "single-instance lock failed errno=" << guard.error_code();
            exitCode = 1;
        }
        else if (!guard.is_primary())
        {
            STACKLOG_INFO() << "another instance is already running";
            exitCode = 0;
        }
        else
        {
            lifegame::view::theme::apply(application);

            {
                lifegame::app::MainWindow window;
                window.show();
            installAutoClose(&window);
                exitCode = application.exec();
            }
            STACKLOG_INFO() << "event loop exited code=" << exitCode;
            STACKLOG_INFO() << "shutdown";
        }
    }

    stacklog::shutdown();
    return exitCode;
}
