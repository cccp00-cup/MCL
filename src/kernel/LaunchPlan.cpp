#include "LaunchPlan.h"
#include "Mirror.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QSet>
#include <QJsonObject>

#ifdef Q_OS_WIN
constexpr QChar kPathSep = QLatin1Char(';');
#else
constexpr QChar kPathSep = QLatin1Char(':');
#endif

namespace
{
const char *kResourcesBase = "https://resources.download.minecraft.net/";
}

QString LaunchPlan::offlineUuid(const QString &playerName)
{
    // 离线模式的 UUID 是约定俗成的算法：MD5("OfflinePlayer:<名字>")，
    // 再把第 13 位改成 '3'（version 3 = name-based），第 17 位按 variant 掩码处理。
    const QByteArray raw =
        QCryptographicHash::hash(("OfflinePlayer:" + playerName).toUtf8(), QCryptographicHash::Md5)
            .toHex();
    QString hex = QString::fromLatin1(raw);
    if (hex.size() < 32)
        return QStringLiteral("00000000000000000000000000000000");

    hex[12] = QLatin1Char('3');
    const char ch = hex.at(16).toLatin1();
    hex[16] = QLatin1String("89ab")[int(ch) & 0x3];

    // 拼成 8-4-4-4-12
    return hex.left(8) + QLatin1Char('-') + hex.mid(8, 4) + QLatin1Char('-') + hex.mid(12, 4)
        + QLatin1Char('-') + hex.mid(16, 4) + QLatin1Char('-') + hex.mid(20, 12);
}

QList<DownloadItem> LaunchPlan::collectVersionFiles(const McVersionInfo &version,
                                                    const QString &versionsDir,
                                                    const QString &librariesDir,
                                                    const QString &assetsDir)
{
    QList<DownloadItem> items;
    const QMap<QString, bool> features = McVersion::defaultFeatures();

    // 按目标文件去重。真实例子里就有：1.12.2 的 version.json 里
    // ca.weblite:java-objc-bridge:1.0.0 出现了两次（两条不同的 rules），
    // 不去重就会有两个线程抢同一个 .part 文件，先改完名的那个会把文件搬走，
    // 后一个直接"改名失败"。
    QSet<QString> seenTargets;
    auto push = [&items, &seenTargets](const DownloadItem &item) {
        if (item.targetPath.isEmpty() || seenTargets.contains(item.targetPath))
            return;
        seenTargets.insert(item.targetPath);
        items << item;
    };

    if (!version.clientUrl.isEmpty()) {
        DownloadItem item;
        item.url = Mirror::rewrite(version.clientUrl);
        item.fallbackUrl = version.clientUrl;
        item.targetPath = versionsDir + QLatin1Char('/') + version.id + QLatin1Char('/')
            + version.id + QStringLiteral(".jar");
        item.sha1 = version.clientSha1;
        item.size = version.clientSize;
        item.label = version.id + QStringLiteral(".jar");
        push(item);
    }

    for (const McLibrary &lib : version.libraries) {
        if (!lib.appliesTo(features))
            continue;
        if (lib.url.isEmpty() || lib.path.isEmpty())
            continue; // 没有下载地址的库由别的库提供，跳过
        DownloadItem item;
        item.url = Mirror::rewrite(lib.url);
        item.fallbackUrl = lib.url;
        item.targetPath = librariesDir + QLatin1Char('/') + lib.path;
        item.sha1 = lib.sha1;
        item.size = lib.size;
        item.label = lib.name;
        push(item);
    }

    if (!version.assetIndex.url.isEmpty()) {
        DownloadItem item;
        item.url = Mirror::rewrite(version.assetIndex.url);
        item.fallbackUrl = version.assetIndex.url;
        item.targetPath = assetsDir + QStringLiteral("/indexes/") + version.assetIndex.id
            + QStringLiteral(".json");
        item.sha1 = version.assetIndex.sha1;
        item.size = version.assetIndex.size;
        item.label = QStringLiteral("asset index %1").arg(version.assetIndex.id);
        push(item);
    }

    return items;
}

QList<DownloadItem> LaunchPlan::collectAssets(const QString &indexPath, const QString &assetsDir)
{
    QList<DownloadItem> items;
    QSet<QString> seenHashes; // 不同资源名可能指向同一个对象

    QFile file(indexPath);
    if (!file.open(QIODevice::ReadOnly))
        return items;

    const QJsonObject objects =
        QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("objects")).toObject();

    for (auto it = objects.constBegin(); it != objects.constEnd(); ++it) {
        const QJsonObject object = it.value().toObject();
        const QString hash = object.value(QStringLiteral("hash")).toString();
        if (hash.size() < 2)
            continue;

        if (seenHashes.contains(hash))
            continue;
        seenHashes.insert(hash);

        const QString prefix = hash.left(2);
        DownloadItem item;
        const QString assetUrl =
            QString::fromLatin1(kResourcesBase) + prefix + QLatin1Char('/') + hash;
        item.url = Mirror::rewrite(assetUrl);
        item.fallbackUrl = assetUrl;
        item.targetPath = assetsDir + QStringLiteral("/objects/") + prefix + QLatin1Char('/') + hash;
        item.sha1 = hash; // 资源文件名就是 SHA1
        item.size = qint64(object.value(QStringLiteral("size")).toDouble(-1));
        item.label = QStringLiteral("asset %1").arg(it.key());
        items << item;
    }

    return items;
}

QStringList LaunchPlan::classpathEntries(const McVersionInfo &version, const QString &clientJar,
                                         const QString &librariesDir)
{
    const QMap<QString, bool> features = McVersion::defaultFeatures();

    QStringList entries;
    entries << clientJar;

    for (const McLibrary &lib : version.libraries) {
        if (!lib.appliesTo(features))
            continue;
        // natives 包不进 classpath，它只用来解压出 .so/.dll/.dylib
        if (lib.isNative)
            continue;
        if (lib.path.isEmpty())
            continue;
        entries << librariesDir + QLatin1Char('/') + lib.path;
    }

    return entries;
}

QStringList LaunchPlan::buildArguments(const McVersionInfo &version, const QString &playerName,
                                       const QString &playerUuid, const QString &accessToken,
                                       const QString &userType, const QString &gameDir,
                                       const QString &assetsDir,
                                       const QString &nativesDir, const QString &librariesDir,
                                       const QString &clientJar, int memoryMb)
{
    const QMap<QString, bool> features = McVersion::defaultFeatures();
    const QString separator = QString(kPathSep);

    const QMap<QString, QString> vars = {
        { QStringLiteral("auth_player_name"), playerName },
        { QStringLiteral("auth_uuid"), playerUuid },
        { QStringLiteral("auth_access_token"), accessToken },
        { QStringLiteral("auth_session"), QStringLiteral("0") },
        { QStringLiteral("auth_xuid"), QStringLiteral("0") },
        { QStringLiteral("clientid"), QStringLiteral("0") },
        { QStringLiteral("user_type"), userType },
        { QStringLiteral("user_properties"), QStringLiteral("{}") },
        { QStringLiteral("version_name"), version.id },
        { QStringLiteral("version_type"), version.type },
        { QStringLiteral("game_directory"), gameDir },
        { QStringLiteral("assets_root"), assetsDir },
        { QStringLiteral("assets_index_name"), version.assetIndex.id },
        { QStringLiteral("natives_directory"), nativesDir },
        { QStringLiteral("library_directory"), librariesDir },
        { QStringLiteral("classpath_separator"), separator },
        // 少了这个，jvm 参数里的 -cp ${classpath} 会原样传下去，游戏自然找不到主类
        { QStringLiteral("classpath"),
          classpathEntries(version, clientJar, librariesDir).join(separator) },
        { QStringLiteral("launcher_name"), QStringLiteral("mcl") },
        { QStringLiteral("launcher_version"), QStringLiteral("0.1") },
        { QStringLiteral("resolution_width"), QStringLiteral("1280") },
        { QStringLiteral("resolution_height"), QStringLiteral("800") },
        { QStringLiteral("quickPlayPath"), QString() },
        { QStringLiteral("quickPlaySingleplayer"), QString() },
        { QStringLiteral("quickPlayMultiplayer"), QString() },
        { QStringLiteral("quickPlayRealms"), QString() },
    };

    QStringList args;

    // —— JVM 参数：直接来自 version.json（1.13+ 有 jvm 数组）
    for (const McArgument &arg : version.jvmArguments) {
        // 没有 rules 的参数是无条件适用的；只有带了 rules 才需要求值
        if (!arg.rules.isEmpty() && !McRule::evaluate(arg.rules, features, false))
            continue;
        for (const QString &value : arg.values)
            args << McVersion::substitute(value, vars);
    }

    // 官方 launcher 会在没给内存参数时用默认值，这里补上
    bool hasXmx = false;
    for (const QString &a : args) {
        if (a.startsWith(QLatin1String("-Xmx")))
            hasXmx = true;
    }
    if (!hasXmx) {
        args << QStringLiteral("-Xmx%1M").arg(memoryMb);
        args << QStringLiteral("-Xms512M");
    }

    // `-cp` 与 `-Djava.library.path`：1.13+ 的 jvm 数组里已经带了，只有旧版本
    // （没有 jvm 数组）才需要我们自己补；否则会重复一次 -cp。
    if (version.jvmArguments.isEmpty()) {
        args << QStringLiteral("-Djava.library.path=%1").arg(nativesDir);
        args << QStringLiteral("-cp");
        args << classpathEntries(version, clientJar, librariesDir).join(separator);
    }
    args << version.mainClass;

    // —— 游戏参数
    for (const McArgument &arg : version.gameArguments) {
        if (!arg.rules.isEmpty() && !McRule::evaluate(arg.rules, features, false))
            continue;
        for (const QString &value : arg.values) {
            const QString substituted = McVersion::substitute(value, vars);
            // 会被替换成空的占位（如 quickPlayPath）直接丢掉，否则游戏会拿到一个空参数
            if (substituted.isEmpty() && value.startsWith(QLatin1String("${")))
                continue;
            args << substituted;
        }
    }

    return args;
}
