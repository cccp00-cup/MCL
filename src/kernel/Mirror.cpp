#include "Mirror.h"

namespace
{
// 默认走 BMCLAPI：实测在这个网络环境里它覆盖面最全、速度也够。
Mirror::Source g_source = Mirror::Source::Bmclapi;

struct Rule
{
    const char *from;
    const char *to;
};

// —— BMCLAPI：全部收敛到同一个域名，靠路径前缀区分服务
const Rule kBmclapiRules[] = {
    { "https://launchermeta.mojang.com", "https://bmclapi2.bangbang93.com" },
    { "https://piston-meta.mojang.com", "https://bmclapi2.bangbang93.com" },
    { "https://piston-data.mojang.com", "https://bmclapi2.bangbang93.com" },
    { "https://libraries.minecraft.net", "https://bmclapi2.bangbang93.com/maven" },
    { "https://resources.download.minecraft.net", "https://bmclapi2.bangbang93.com/assets" },
    { "https://meta.fabricmc.net", "https://bmclapi2.bangbang93.com/fabric-meta" },
    { "https://maven.fabricmc.net", "https://bmclapi2.bangbang93.com/maven" },
};

// —— FastMCMirror：一个服务一个子域。
// 它没有镜像 piston-meta / piston-data（client.jar 就在后者上），这几个域名只能落回官方。
const Rule kFastMcMirrorRules[] = {
    { "https://launchermeta.mojang.com", "https://launchermeta.fastmcmirror.org" },
    { "https://libraries.minecraft.net", "https://libraries.fastmcmirror.org" },
    { "https://resources.download.minecraft.net", "https://resources.fastmcmirror.org" },
    { "https://maven.minecraftforge.net", "https://forge.fastmcmirror.org" },
    { "https://files.minecraftforge.net/maven", "https://forge.fastmcmirror.org" },
    { "https://meta.fabricmc.net", "https://fabricmeta.fastmcmirror.org" },
    { "https://maven.fabricmc.net", "https://fabric.fastmcmirror.org" },
};

template<int N>
QString applyRules(const Rule (&rules)[N], const QString &url)
{
    for (const Rule &rule : rules) {
        const QString from = QString::fromLatin1(rule.from);
        if (url.startsWith(from))
            return QString::fromLatin1(rule.to) + url.mid(from.size());
    }
    return url;
}
} // namespace

namespace Mirror
{

void setSource(Source source)
{
    g_source = source;
}

Source source()
{
    return g_source;
}

Source fromId(const QString &id)
{
    const QString lower = id.toLower();
    if (lower == QLatin1String("official"))
        return Source::Official;
    if (lower == QLatin1String("fastmcmirror"))
        return Source::FastMCMirror;
    return Source::Bmclapi;
}

QString sourceId(Source source)
{
    switch (source) {
    case Source::Official:
        return QStringLiteral("official");
    case Source::FastMCMirror:
        return QStringLiteral("fastmcmirror");
    case Source::Bmclapi:
        break;
    }
    return QStringLiteral("bmclapi");
}

QString sourceName()
{
    switch (g_source) {
    case Source::Official:
        return QStringLiteral("官方源");
    case Source::FastMCMirror:
        return QStringLiteral("FastMCMirror");
    case Source::Bmclapi:
        break;
    }
    return QStringLiteral("BMCLAPI");
}

QString rewrite(const QString &url)
{
    if (url.isEmpty() || g_source == Source::Official)
        return url;

    if (g_source == Source::Bmclapi)
        return applyRules(kBmclapiRules, url);
    return applyRules(kFastMcMirrorRules, url);
}

} // namespace Mirror
