#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

#include "app/logging.h"
#include "app/main_window.h"
#include "view/theme.h"

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
                                           QStringLiteral("Enable lifegame.* debug logging."));
    const QCommandLineOption logFileOption(QStringLiteral("log-file"),
                                           QStringLiteral("Write logs to this path."),
                                           QStringLiteral("path"));
    parser.addOption(verboseOption);
    parser.addOption(logFileOption);
    parser.process(application);

    lifegame::app::logging::Options loggingOptions;
    loggingOptions.verbose = parser.isSet(verboseOption);
    loggingOptions.filePath = parser.value(logFileOption);
    const bool logFileReady = lifegame::app::logging::initialize(loggingOptions);

    qCInfo(lcApp) << "starting life-game" << QCoreApplication::applicationVersion() << "Qt" << qVersion()
                  << "pid" << QCoreApplication::applicationPid() << "log" << lifegame::app::logging::logFilePath();
    if (!logFileReady)
    {
        qCWarning(lcApp) << "file logging is unavailable; using stderr only";
    }

    lifegame::view::theme::apply(application);

    int exitCode = 0;
    {
        lifegame::app::MainWindow window;
        window.show();
        exitCode = application.exec();
        qCInfo(lcApp) << "event loop exited code=" << exitCode;
    }

    qCInfo(lcApp) << "shutdown";
    lifegame::app::logging::shutdown();
    return exitCode;
}
