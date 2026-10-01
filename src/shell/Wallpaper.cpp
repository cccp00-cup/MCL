#include "Wallpaper.h"

#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace
{
const char *kImgExts[] = { "png", "jpg", "jpeg", "webp", "avif", "bmp", "gif", "svg" };

bool isImageFile(const QString &p)
{
    for (const char *e : kImgExts)
        if (p.endsWith(QLatin1Char('.') + QLatin1String(e), Qt::CaseInsensitive))
            return true;
    return false;
}

// Debian alternatives 等会用 XML 描述壁纸，挑选最接近 16:10 横屏的真实图片。
QString imageFromXml(const QString &xmlPath)
{
    QFile f(xmlPath);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    const QString text = QString::fromUtf8(f.readAll());
    static const QRegularExpression re(
        QStringLiteral("([^\"'<>\\s]+\\.(?:png|jpe?g|webp|avif|gif|svg))"),
        QRegularExpression::CaseInsensitiveOption);

    QString best;
    double bestScore = 1e9;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        QString candidate = it.next().captured(1);
        if (QDir::isRelativePath(candidate))
            candidate = QFileInfo(xmlPath).absolutePath() + QLatin1Char('/') + candidate;
        if (!QFile::exists(candidate))
            continue;

        double score = 0.1; // 无尺寸信息的候选也能兜底
        static const QRegularExpression sizeRe(QStringLiteral("(\\d+)x(\\d+)\\.[a-z0-9]+$"),
                                               QRegularExpression::CaseInsensitiveOption);
        const auto m = sizeRe.match(candidate);
        if (m.hasMatch()) {
            const double w = m.captured(1).toInt();
            const double h = m.captured(2).toInt();
            if (w < h)
                continue; // 竖屏图直接跳过
            score = std::abs((w / h) - 1.6); // 窗口比例 16:10
        }
        if (score < bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

// --- Linux / BSD ---
QString fromPlasmaConfig()
{
    const QString cfg = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/plasma-org.kde.plasma.desktop-appletsrc");
    QFile f(cfg);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    QString last;
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.startsWith(QLatin1String("Image="), Qt::CaseInsensitive)) {
            const QString v = line.mid(6);
            if (!v.isEmpty() && v.compare(QLatin1String("None"), Qt::CaseInsensitive) != 0)
                last = v;
        }
    }
    if (last.isEmpty())
        return {};
    const QUrl url(last);
    QString p = url.isLocalFile() ? url.toLocalFile() : last;
    if (!QFile::exists(p))
        return {};
    if (!isImageFile(p))
        p = imageFromXml(p);
    return p;
}

// 兜底：扫 plasmashell 进程打开的 fd，找图片文件（plasma-appletsrc 可能不含 Image 配置）
QString fromPlasmaProc()
{
    QDir proc(QStringLiteral("/proc"));
    const auto pids = proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &pid : pids) {
        bool ok = false;
        pid.toLongLong(&ok);
        if (!ok)
            continue;
        QFile cl(QStringLiteral("/proc/") + pid + QStringLiteral("/cmdline"));
        if (!cl.open(QIODevice::ReadOnly) || !cl.readAll().contains("plasmashell"))
            continue;
        QDir fdDir(QStringLiteral("/proc/") + pid + QStringLiteral("/fd"));
        const auto fds = fdDir.entryList(QDir::Files);
        QString best;
        qint64 bestSize = 100 * 1024; // 忽略小图标
        for (const QString &fd : fds) {
            const QString target =
                QFileInfo(QStringLiteral("/proc/") + pid + QStringLiteral("/fd/") + fd).symLinkTarget();
            if (!isImageFile(target))
                continue;
            const QFileInfo fi(target);
            if (!fi.isReadable())
                continue;
            if (fi.size() > bestSize) {
                bestSize = fi.size();
                best = target;
            }
        }
        if (!best.isEmpty())
            return best;
    }
    return {};
}

QString fromGsettings()
{
    for (const char *key : { "picture-uri-dark", "picture-uri" }) {
        QProcess p;
        p.start(QStringLiteral("gsettings"),
                { QStringLiteral("get"), QStringLiteral("org.gnome.desktop.background"),
                  QLatin1String(key) });
        if (!p.waitForStarted(2000) || !p.waitForFinished(3000))
            continue;
        QString out = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (out.startsWith(QLatin1Char('\'')))
            out = out.mid(1);
        if (out.endsWith(QLatin1Char('\'')))
            out.chop(1);
        const QUrl url(out);
        QString p2 = url.isLocalFile() ? url.toLocalFile() : out;
        if (!QFile::exists(p2))
            continue;
        if (!isImageFile(p2))
            p2 = imageFromXml(p2);
        if (!p2.isEmpty())
            return p2;
    }
    return {};
}

QString fromXdgWallpapers()
{
    QDir wd(QStringLiteral("/usr/share/wallpapers"));
    for (const QString &d : wd.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        QDir sub(wd.filePath(d + QStringLiteral("/contents")));
        for (const QString &f : sub.entryList(QDir::Files, QDir::Name)) {
            if (isImageFile(f))
                return sub.absoluteFilePath(f);
        }
    }
    return {};
}

} // namespace

QString Wallpaper::detect()
{
#if defined(Q_OS_WIN)
    QSettings s(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Desktop"), QSettings::NativeFormat);
    const QString p = s.value(QStringLiteral("WallPaper")).toString();
    if (!p.isEmpty() && QFile::exists(p))
        return p;
    return {};
#elif defined(Q_OS_MAC)
    QProcess p;
    p.start(QStringLiteral("osascript"),
            { QStringLiteral("-e"),
              QStringLiteral("tell application \"System Events\" to get picture of every desktop") });
    if (p.waitForStarted(2000) && p.waitForFinished(5000)) {
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        for (QString line : out.split(QLatin1Char('\n'))) {
            line = line.trimmed();
            if (line.endsWith(QLatin1Char(',')))
                line.chop(1);
            if (line.startsWith(QLatin1String("file ")))
                line = line.mid(5);
            if (QFile::exists(line))
                return line;
        }
    }
    return {};
#else
    QString p = fromPlasmaConfig();
    if (p.isEmpty())
        p = fromPlasmaProc();
    if (p.isEmpty())
        p = fromGsettings();
    if (p.isEmpty())
        p = fromXdgWallpapers();
    return p;
#endif
}
