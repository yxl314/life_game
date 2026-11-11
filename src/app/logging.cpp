#include "app/logging.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

#include <cstdio>
#include <cstdlib>

namespace lifegame::app::logging
{

namespace
{

constexpr qint64 kMaxLogBytes = 2LL * 1024 * 1024;
constexpr int kRetainedLogs = 3;

QMutex g_mutex;
QFile g_file;
QtMessageHandler g_previous = nullptr;
bool g_fileReady = false;
bool g_mirror = true;
QString g_path;

void rotateIfNeeded(const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists() || info.size() < kMaxLogBytes)
    {
        return;
    }

    const QString oldest = path + QStringLiteral(".%1").arg(kRetainedLogs - 1);
    QFile::remove(oldest);
    for (int index = kRetainedLogs - 2; index >= 1; --index)
    {
        QFile::rename(path + QStringLiteral(".%1").arg(index), path + QStringLiteral(".%1").arg(index + 1));
    }
    QFile::rename(path, path + QStringLiteral(".1"));
}

char levelChar(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return 'D';
    case QtInfoMsg:
        return 'I';
    case QtWarningMsg:
        return 'W';
    case QtCriticalMsg:
        return 'C';
    case QtFatalMsg:
        return 'F';
    }
    return '?';
}

void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    QByteArray line = QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toUtf8();
    line += " [";
    line += levelChar(type);
    line += "] ";
    line += (context.category != nullptr) ? context.category : "default";
    line += ": ";
    line += message.toUtf8();
    if (context.file != nullptr)
    {
        line += " (";
        line += QFileInfo(QString::fromUtf8(context.file)).fileName().toUtf8();
        line += ':';
        line += QByteArray::number(context.line);
        line += ')';
    }
    line += '\n';

    {
        QMutexLocker locker(&g_mutex);
        if (g_fileReady && g_file.isOpen())
        {
            g_file.write(line);
            g_file.flush();
        }
        if (g_mirror || !g_fileReady || type == QtFatalMsg)
        {
            (void)std::fwrite(line.constData(), 1, static_cast<std::size_t>(line.size()), stderr);
            (void)std::fflush(stderr);
        }
    }

    if (type == QtFatalMsg)
    {
        if (g_previous != nullptr)
        {
            g_previous(type, context, message);
        }
        else
        {
            std::abort();
        }
    }
}

} // namespace

bool initialize(const Options& options)
{
    shutdown();

    QMutexLocker locker(&g_mutex);

    QString path = options.filePath;
    if (path.isEmpty())
    {
        const QString directory =
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
        path = directory + QStringLiteral("/life-game.log");
    }

    QDir().mkpath(QFileInfo(path).absolutePath());
    rotateIfNeeded(path);

    g_path = path;
    g_mirror = options.mirrorToStderr;
    g_file.setFileName(path);
    g_fileReady = g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    if (!g_fileReady)
    {
        (void)std::fprintf(stderr, "lifegame: cannot open log file %s\n", path.toUtf8().constData());
        g_mirror = true;
    }

    QLoggingCategory::setFilterRules(options.verbose ? QStringLiteral("*.debug=false\nlifegame.*.debug=true")
                                                     : QStringLiteral("*.debug=false"));
    g_previous = qInstallMessageHandler(messageHandler);
    return g_fileReady;
}

void shutdown()
{
    QMutexLocker locker(&g_mutex);
    if (g_previous != nullptr)
    {
        qInstallMessageHandler(g_previous);
        g_previous = nullptr;
    }
    if (g_file.isOpen())
    {
        g_file.flush();
        g_file.close();
    }
    g_fileReady = false;
}

QString logFilePath()
{
    QMutexLocker locker(&g_mutex);
    return g_path;
}

} // namespace lifegame::app::logging

Q_LOGGING_CATEGORY(lcApp, "lifegame.app")
