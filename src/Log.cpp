#include "Log.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QStandardPaths>

#include <cstdio>

#ifdef Q_OS_WIN
#include <QMessageLogContext>
#include <QtGlobal>
#endif

namespace {

#ifdef Q_OS_WIN

// 单个日志文件的上限，超了就转成 mcl.log.1（只留一代，够用且不会撑爆磁盘）
constexpr qint64 kMaxBytes = 2 * 1024 * 1024;

QMutex g_mutex;
QFile g_file;
bool g_ready = false;

QString logFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/mcl.log");
}

// 调之前必须已持有 g_mutex
void rotateIfNeeded()
{
    if (g_file.size() < kMaxBytes)
        return;

    const QString path = g_file.fileName();
    g_file.close();
    QFile::remove(path + QStringLiteral(".1"));
    QFile::rename(path, path + QStringLiteral(".1"));
    g_file.setFileName(path);
    g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void writeToFile(const QString &text)
{
    QMutexLocker locker(&g_mutex);
    if (!g_ready)
        return;

    rotateIfNeeded();
    if (!g_file.isOpen())
        return;

    // 带时间戳 —— 排查启动问题时"哪一步慢、哪一步重复"全靠它
    const QString stamped = QDateTime::currentDateTime().toString(QStringLiteral("MM-dd HH:mm:ss.zzz "))
                            + text;
    g_file.write(stamped.toUtf8());
    g_file.write("\n");
    g_file.flush();
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &message)
{
    const char *tag = "";
    switch (type) {
    case QtDebugMsg:
    case QtInfoMsg:
        break;
    case QtWarningMsg:
        tag = "[warn] ";
        break;
    case QtCriticalMsg:
        tag = "[crit] ";
        break;
    case QtFatalMsg:
        tag = "[fatal] ";
        break;
    }

    writeToFile(QString::fromLatin1(tag) + message);

    if (type == QtFatalMsg)
        abort();
}

#endif // Q_OS_WIN

} // namespace

QString Log::filePath()
{
#ifdef Q_OS_WIN
    return logFilePath();
#else
    return {};
#endif
}

void Log::install()
{
#ifdef Q_OS_WIN
    QMutexLocker locker(&g_mutex);

    const QString path = logFilePath();

    // 上一次跑剩的如果已经到上限，先转走再开新的
    if (QFileInfo::exists(path) && QFileInfo(path).size() >= kMaxBytes) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }

    g_file.setFileName(path);
    g_ready = g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    if (!g_ready)
        return;

    g_file.write(QStringLiteral("\n=== mcl 启动 ")
                     .append(QDateTime::currentDateTime().toString(Qt::ISODate))
                     .append(QStringLiteral(" ===\n"))
                     .toUtf8());
    g_file.flush();

    locker.unlock();

    // Qt / QML 自己的警告也接过来 —— QML 报的错在 Windows 上同样看不见
    qInstallMessageHandler(messageHandler);
#endif
}

void Log::line(const QString &text)
{
#ifdef Q_OS_WIN
    writeToFile(text);
#else
    // 非 Windows 保持原样：终端里直接看
    fprintf(stderr, "%s\n", qPrintable(text));
    fflush(stderr);
#endif
}
