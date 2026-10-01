#include "JavaLocator.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

namespace
{
QString exeName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("java.exe");
#else
    return QStringLiteral("java");
#endif
}

void addCandidate(QSet<QString> &seen, QStringList &out, const QString &dir)
{
    if (dir.isEmpty())
        return;
    const QString path = QDir(dir).filePath(exeName());
    if (!QFileInfo::exists(path))
        return;
    const QString canonical = QFileInfo(path).canonicalFilePath();
    if (canonical.isEmpty() || seen.contains(canonical))
        return;
    seen.insert(canonical);
    out << canonical;
}

// 在 parent 的每个子目录下拼 <sub>/bin/java
void addFromChildDirs(QSet<QString> &seen, QStringList &out, const QString &parent,
                      const QString &relative)
{
    QDir dir(parent);
    if (!dir.exists())
        return;
    const QStringList children = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &child : children)
        addCandidate(seen, out, dir.filePath(child + QLatin1Char('/') + relative));
}
} // namespace

QString JavaInstall::label() const
{
    if (!isValid())
        return QStringLiteral("—");
    return QStringLiteral("Java %1%2").arg(
        QString::number(major),
        vendor.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(vendor));
}

namespace JavaLocator
{

JavaInstall probe(const QString &javaExecutable)
{
    JavaInstall install;
    install.path = javaExecutable;
    if (javaExecutable.isEmpty() || !QFileInfo::exists(javaExecutable))
        return install;

    // 先读 JAVA_HOME/release —— 又快又稳，不必起进程
    const QDir binDir = QFileInfo(javaExecutable).absoluteDir();
    QFile release(binDir.filePath(QStringLiteral("../release")));
    if (release.open(QIODevice::ReadOnly)) {
        const QString text = QString::fromUtf8(release.readAll());
        static const QRegularExpression re(QStringLiteral("JAVA_VERSION=\"([^\"]+)\""));
        const auto m = re.match(text);
        if (m.hasMatch()) {
            install.version = m.captured(1);
            const QStringList parts = install.version.split(QLatin1Char('.'));
            if (!parts.isEmpty()) {
                int first = parts.at(0).toInt();
                if (first == 1 && parts.size() > 1)
                    first = parts.at(1).toInt();
                install.major = first;
            }
            if (text.contains(QLatin1String("Temurin")))
                install.vendor = QStringLiteral("Temurin");
            else if (text.contains(QLatin1String("OpenJDK")))
                install.vendor = QStringLiteral("OpenJDK");
        }
    }

    if (install.isValid())
        return install;

    // 退回实际跑一次 `java -version`（输出在 stderr）
    QProcess process;
    process.start(javaExecutable, { QStringLiteral("-version") });
    if (!process.waitForStarted(3000) || !process.waitForFinished(8000))
        return install;

    const QString text = QString::fromUtf8(process.readAllStandardError())
        + QString::fromUtf8(process.readAllStandardOutput());

    static const QRegularExpression re(QStringLiteral("version \"([^\"]+)\""));
    const auto m = re.match(text);
    if (m.hasMatch()) {
        install.version = m.captured(1);
        const QStringList parts = install.version.split(QLatin1Char('.'));
        if (!parts.isEmpty()) {
            int first = parts.at(0).toInt();
            if (first == 1 && parts.size() > 1)
                first = parts.at(1).toInt();
            install.major = first;
        }
    }
    if (text.contains(QLatin1String("Temurin")))
        install.vendor = QStringLiteral("Temurin");
    else if (text.contains(QLatin1String("GraalVM")))
        install.vendor = QStringLiteral("GraalVM");
    else if (text.contains(QLatin1String("OpenJDK")))
        install.vendor = QStringLiteral("OpenJDK");

    return install;
}

QList<JavaInstall> findAll()
{
    QSet<QString> seen;
    QStringList candidates;

    // JAVA_HOME 优先
    const QByteArray javaHome = qgetenv("JAVA_HOME");
    if (!javaHome.isEmpty())
        addCandidate(seen, candidates, QString::fromLocal8Bit(javaHome) + QStringLiteral("/bin"));

    // PATH 里那个
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (!onPath.isEmpty())
        addCandidate(seen, candidates, QFileInfo(onPath).absolutePath());

#if defined(Q_OS_MACOS)
    addFromChildDirs(seen, candidates, QStringLiteral("/Library/Java/JavaVirtualMachines"),
                     QStringLiteral("Contents/Home/bin"));
    addFromChildDirs(seen, candidates, QStringLiteral("/System/Volumes/Data/Library/Java/JavaVirtualMachines"),
                     QStringLiteral("Contents/Home/bin"));
#elif defined(Q_OS_WIN)
    addFromChildDirs(seen, candidates, QStringLiteral("C:/Program Files/Java"), QStringLiteral("bin"));
    addFromChildDirs(seen, candidates, QStringLiteral("C:/Program Files/Eclipse Adoptium"), QStringLiteral("bin"));
    addFromChildDirs(seen, candidates, QStringLiteral("C:/Program Files/Microsoft"), QStringLiteral("bin"));
#endif

    addFromChildDirs(seen, candidates, QStringLiteral("/usr/lib/jvm"), QStringLiteral("bin"));
    addFromChildDirs(seen, candidates, QStringLiteral("/usr/java"), QStringLiteral("bin"));
    addFromChildDirs(seen, candidates, QStringLiteral("/opt/java"), QStringLiteral("bin"));

    const QString home = QDir::homePath();
    addFromChildDirs(seen, candidates, home + QStringLiteral("/.sdkman/candidates/java"),
                     QStringLiteral("bin"));
    addFromChildDirs(seen, candidates, home + QStringLiteral("/.jdks"), QStringLiteral("bin"));

    QList<JavaInstall> result;
    for (const QString &path : candidates) {
        const JavaInstall install = probe(path);
        if (install.isValid())
            result << install;
    }

    std::sort(result.begin(), result.end(), [](const JavaInstall &a, const JavaInstall &b) {
        return a.major > b.major;
    });
    return result;
}

JavaInstall findBest(int requiredMajor)
{
    const QList<JavaInstall> all = findAll();
    if (all.isEmpty())
        return {};

    // 1) 主版本正好对上
    for (const JavaInstall &j : all) {
        if (j.major == requiredMajor)
            return j;
    }
    // 2) 比要求新，取其中最小的那个
    JavaInstall newer;
    for (const JavaInstall &j : all) {
        if (j.major > requiredMajor && (!newer.isValid() || j.major < newer.major))
            newer = j;
    }
    if (newer.isValid())
        return newer;

    // 3) 只能拿旧的凑（大概率跑不起来，但至少给出一个选择）
    JavaInstall older;
    for (const JavaInstall &j : all) {
        if (j.major < requiredMajor && (!older.isValid() || j.major > older.major))
            older = j;
    }
    return older;
}

} // namespace JavaLocator
