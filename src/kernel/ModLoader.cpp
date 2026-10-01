#include "ModLoader.h"

#include <QRegularExpression>

namespace
{
// maven-metadata.xml 里的版本列表
QStringList versionsFromMetadata(const QByteArray &xml)
{
    QStringList out;
    static const QRegularExpression re(QStringLiteral("<version>([^<]+)</version>"));
    auto it = re.globalMatch(QString::fromUtf8(xml));
    while (it.hasNext())
        out << it.next().captured(1);
    return out;
}

// 1.20.1 → "20.1"；1.21 → "21.0"。NeoForge 用它当版本前缀
QString neoForgePrefix(const QString &gameVersion)
{
    const QStringList parts = gameVersion.split(QLatin1Char('.'));
    if (parts.size() < 2)
        return {};
    const QString minor = parts.at(1);
    const QString patch = parts.size() > 2 ? parts.at(2) : QStringLiteral("0");
    return minor + QLatin1Char('.') + patch;
}

// 版本号按数字段比较大小
bool versionLess(const QString &a, const QString &b)
{
    const QStringList pa = a.split(QRegularExpression(QStringLiteral("[.\\-+]")));
    const QStringList pb = b.split(QRegularExpression(QStringLiteral("[.\\-+]")));
    for (int i = 0; i < qMax(pa.size(), pb.size()); ++i) {
        const int va = i < pa.size() ? pa.at(i).toInt() : 0;
        const int vb = i < pb.size() ? pb.at(i).toInt() : 0;
        if (va != vb)
            return va < vb;
    }
    return false;
}
} // namespace

namespace ModLoader
{

Kind fromId(const QString &id)
{
    const QString lower = id.toLower();
    if (lower == QLatin1String("fabric"))
        return Kind::Fabric;
    if (lower == QLatin1String("forge"))
        return Kind::Forge;
    if (lower == QLatin1String("neoforge"))
        return Kind::NeoForge;
    return Kind::Vanilla;
}

QString displayName(Kind kind)
{
    switch (kind) {
    case Kind::Fabric:
        return QStringLiteral("Fabric");
    case Kind::Forge:
        return QStringLiteral("Forge");
    case Kind::NeoForge:
        return QStringLiteral("NeoForge");
    case Kind::Vanilla:
        break;
    }
    return QStringLiteral("Vanilla");
}

QString metadataUrl(Kind kind)
{
    switch (kind) {
    case Kind::Forge:
        // 注意：BMCLAPI 那份 Forge 元数据是陈旧的（最新只到 1.18），
        // 官方这份反而是通的，所以这里固定用官方。
        return QStringLiteral(
            "https://maven.minecraftforge.net/net/minecraftforge/forge/maven-metadata.xml");
    case Kind::NeoForge:
        return QStringLiteral(
            "https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
    default:
        break;
    }
    return {};
}

QString pickVersion(Kind kind, const QByteArray &metadataXml, const QString &gameVersion)
{
    const QStringList all = versionsFromMetadata(metadataXml);
    QString best;

    if (kind == Kind::Forge) {
        // Forge 的版本形如 "1.20.1-47.4.9"，前缀就是游戏版本
        const QString prefix = gameVersion + QLatin1Char('-');
        for (const QString &v : all) {
            if (!v.startsWith(prefix))
                continue;
            if (best.isEmpty() || versionLess(best, v))
                best = v;
        }
        return best;
    }

    if (kind == Kind::NeoForge) {
        // NeoForge 的版本形如 "20.1.100"，对应 1.20.1
        const QString prefix = neoForgePrefix(gameVersion) + QLatin1Char('.');
        for (const QString &v : all) {
            if (!v.startsWith(prefix))
                continue;
            if (best.isEmpty() || versionLess(best, v))
                best = v;
        }
        return best;
    }

    return {};
}

QString installerUrl(Kind kind, const QString &gameVersion, const QString &loaderVersion)
{
    if (kind == Kind::Forge) {
        // https://maven.minecraftforge.net/net/minecraftforge/forge/1.20.1-47.4.9/forge-1.20.1-47.4.9-installer.jar
        return QStringLiteral("https://maven.minecraftforge.net/net/minecraftforge/forge/%1/"
                              "forge-%1-installer.jar")
            .arg(loaderVersion);
    }
    if (kind == Kind::NeoForge) {
        // https://maven.neoforged.net/releases/net/neoforged/neoforge/20.1.100/neoforge-20.1.100-installer.jar
        return QStringLiteral("https://maven.neoforged.net/releases/net/neoforged/neoforge/%1/"
                              "neoforge-%1-installer.jar")
            .arg(loaderVersion);
    }
    Q_UNUSED(gameVersion)
    return {};
}

} // namespace ModLoader
