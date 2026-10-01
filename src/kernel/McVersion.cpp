#include "McVersion.h"

#include <QHash>
#include <QRegularExpression>
#include <QSysInfo>

namespace
{
QString archName()
{
#if defined(Q_PROCESSOR_X86_64)
    return QStringLiteral("x86_64");
#elif defined(Q_PROCESSOR_ARM_64)
    return QStringLiteral("arm64");
#elif defined(Q_PROCESSOR_X86_32)
    return QStringLiteral("x86");
#else
    return QStringLiteral("unknown");
#endif
}

QString osVersionString()
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    return QSysInfo::productVersion();
#else
    return QSysInfo::kernelVersion();
#endif
}
} // namespace

QString McVersion::currentOsName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("osx");
#else
    return QStringLiteral("linux");
#endif
}

QString McVersion::currentNativesName()
{
    // 注意：natives 的分类名用的是 "osx" 而不是 macos，这是历史遗留
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("osx");
#else
    return QStringLiteral("linux");
#endif
}

QMap<QString, bool> McVersion::defaultFeatures()
{
    // 我们目前不支持 demo 模式 / 自定义分辨率这类特性，一律取 false
    return {};
}

QList<McRule> McRule::parse(const QJsonArray &array)
{
    QList<McRule> out;
    for (const QJsonValue &v : array) {
        const QJsonObject o = v.toObject();
        McRule r;
        r.allow = o.value(QStringLiteral("action")).toString() != QLatin1String("disallow");
        const QJsonObject os = o.value(QStringLiteral("os")).toObject();
        r.osName = os.value(QStringLiteral("name")).toString();
        r.osArch = os.value(QStringLiteral("arch")).toString();
        r.osVersion = os.value(QStringLiteral("version")).toString();
        const QJsonObject feats = o.value(QStringLiteral("features")).toObject();
        for (auto it = feats.constBegin(); it != feats.constEnd(); ++it)
            r.features.insert(it.key(), it.value().toBool());
        out << r;
    }
    return out;
}

bool McRule::matches(const QMap<QString, bool> &activeFeatures) const
{
    if (!osName.isEmpty() && osName != McVersion::currentOsName())
        return false;

    if (!osArch.isEmpty() && osArch != archName())
        return false;

    if (!osVersion.isEmpty()) {
        const QRegularExpression re(osVersion);
        if (!re.match(osVersionString()).hasMatch())
            return false;
    }

    for (auto it = features.constBegin(); it != features.constEnd(); ++it) {
        if (activeFeatures.value(it.key(), false) != it.value())
            return false;
    }

    return true;
}

bool McRule::evaluate(const QList<McRule> &rules, const QMap<QString, bool> &features,
                      bool defaultValue)
{
    // 从前往后，最后一条匹配的规则说了算
    bool result = defaultValue;
    for (const McRule &r : rules) {
        if (r.matches(features))
            result = r.allow;
    }
    return result;
}

bool McLibrary::appliesTo(const QMap<QString, bool> &features) const
{
    // Mojang 的语义，两步都要对：
    //   · **没有 rules** 的条目才是无条件适用；
    //   · 有 rules 但**一条都不匹配**时，结果是"不适用"，不是"适用"。
    // 早先写成 evaluate(rules, features, true)，导致 Linux 上也会去下
    // `:natives-windows`，一路 404 重试把流程拖死。
    if (rules.isEmpty())
        return true;
    return McRule::evaluate(rules, features, false);
}

McVersionInfo McVersion::parseVersion(const QJsonObject &root)
{
    McVersionInfo v;
    v.id = root.value(QStringLiteral("id")).toString();
    v.type = root.value(QStringLiteral("type")).toString();
    v.mainClass = root.value(QStringLiteral("mainClass")).toString();
    v.assets = root.value(QStringLiteral("assets")).toString();
    v.inheritsFrom = root.value(QStringLiteral("inheritsFrom")).toString();

    const QJsonObject downloads = root.value(QStringLiteral("downloads")).toObject();
    const QJsonObject client = downloads.value(QStringLiteral("client")).toObject();
    v.clientUrl = client.value(QStringLiteral("url")).toString();
    v.clientSha1 = client.value(QStringLiteral("sha1")).toString();
    v.clientSize = qint64(client.value(QStringLiteral("size")).toDouble(-1));

    const QJsonObject ai = root.value(QStringLiteral("assetIndex")).toObject();
    v.assetIndex.id = ai.value(QStringLiteral("id")).toString();
    v.assetIndex.url = ai.value(QStringLiteral("url")).toString();
    v.assetIndex.sha1 = ai.value(QStringLiteral("sha1")).toString();
    v.assetIndex.size = qint64(ai.value(QStringLiteral("size")).toDouble(-1));
    v.assetIndex.totalSize = qint64(ai.value(QStringLiteral("totalSize")).toDouble(-1));

    // 0 表示"这个版本描述里没写"。不能默认成 8 —— 模组加载器的 description 就没有
    // 这个字段，默认 8 会把父版本（比如 1.20.1 要的 17）覆盖掉，结果拿 Java 8 去跑。
    v.javaMajor = root.value(QStringLiteral("javaVersion"))
                      .toObject()
                      .value(QStringLiteral("majorVersion"))
                      .toInt(0);

    // 1.13+ 的 arguments 是数组，元素可以是字符串或带 rules 的对象
    auto parseArgList = [](const QJsonArray &arr) {
        QList<McArgument> out;
        for (const QJsonValue &val : arr) {
            McArgument a;
            if (val.isString()) {
                a.values << val.toString();
            } else if (val.isObject()) {
                const QJsonObject o = val.toObject();
                a.rules = McRule::parse(o.value(QStringLiteral("rules")).toArray());
                const QJsonValue value = o.value(QStringLiteral("value"));
                if (value.isString()) {
                    a.values << value.toString();
                } else if (value.isArray()) {
                    for (const QJsonValue &vv : value.toArray())
                        a.values << vv.toString();
                }
            }
            if (!a.values.isEmpty())
                out << a;
        }
        return out;
    };

    const QJsonObject arguments = root.value(QStringLiteral("arguments")).toObject();
    v.jvmArguments = parseArgList(arguments.value(QStringLiteral("jvm")).toArray());
    v.gameArguments = parseArgList(arguments.value(QStringLiteral("game")).toArray());

    // 1.12 及更早：minecraftArguments 是一整条以空格分隔的字符串
    if (v.gameArguments.isEmpty()) {
        const QString legacy = root.value(QStringLiteral("minecraftArguments")).toString();
        const QStringList tokens = legacy.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &tok : tokens)
            v.gameArguments << McArgument{ {}, QStringList{ tok } };
    }

    const QJsonArray libraries = root.value(QStringLiteral("libraries")).toArray();
    for (const QJsonValue &lv : libraries) {
        const QJsonObject lo = lv.toObject();
        McLibrary lib;
        lib.name = lo.value(QStringLiteral("name")).toString();
        lib.rules = McRule::parse(lo.value(QStringLiteral("rules")).toArray());

        const QJsonObject dls = lo.value(QStringLiteral("downloads")).toObject();
        const QJsonObject artifact = dls.value(QStringLiteral("artifact")).toObject();
        lib.path = artifact.value(QStringLiteral("path")).toString();
        lib.url = artifact.value(QStringLiteral("url")).toString();
        lib.sha1 = artifact.value(QStringLiteral("sha1")).toString();
        lib.size = qint64(artifact.value(QStringLiteral("size")).toDouble(-1));

        // 有些版本描述（Fabric 的 profile 就是）只写 `name` + 仓库地址，没有
        // downloads.artifact。这种情况按 Maven 约定自己推路径。
        if (lib.path.isEmpty() && !lib.name.isEmpty()) {
            const QStringList coords = lib.name.split(QLatin1Char(':'));
            if (coords.size() >= 3) {
                const QString group = coords.at(0);
                const QString artifactId = coords.at(1);
                const QString version = coords.at(2);
                QString classifier;
                if (coords.size() > 3)
                    classifier = QLatin1Char('-') + coords.mid(3).join(QLatin1Char('-'));
                lib.path = group.split(QLatin1Char('.'), Qt::SkipEmptyParts).join(QLatin1Char('/'))
                    + QLatin1Char('/') + artifactId + QLatin1Char('/') + version + QLatin1Char('/')
                    + artifactId + QLatin1Char('-') + version + classifier
                    + QStringLiteral(".jar");
            }
        }
        if (lib.url.isEmpty() && lib.path.isEmpty() == false) {
            // 库对象上的 url 是 Maven 仓库根地址，拼上路径才是完整下载地址
            QString repo = lo.value(QStringLiteral("url")).toString();
            if (!repo.isEmpty()) {
                if (!repo.endsWith(QLatin1Char('/')))
                    repo += QLatin1Char('/');
                lib.url = repo + lib.path;
            }
        }

        // natives 有两种写法，都要认：
        //   · 1.18 及更早：靠 `natives` 字段指定当前平台的 classifier
        //   · 1.19 起：natives 是**独立的库条目**，名字里带 ":natives-<平台>" 分类器，
        //     规则交给 rules 过滤 —— 不认这种的话，这些包会被当成普通库塞进 classpath，
        //     而且一个都不解压。
        const bool classifierStyle = lib.name.contains(QLatin1String(":natives-"));

        const QJsonObject natives = lo.value(QStringLiteral("natives")).toObject();
        if (classifierStyle) {
            lib.isNative = true;
            // "org.lwjgl:lwjgl:3.3.1:natives-linux" → "natives-linux"
            lib.nativeClassifier = lib.name.section(QLatin1Char(':'), 3);
        } else if (!natives.isEmpty()) {
            QString classifier = natives.value(currentNativesName()).toString();
            // 少数库用 ${arch} 占位（32/64 位）
            classifier.replace(QStringLiteral("${arch}"),
                               archName() == QLatin1String("x86") ? QStringLiteral("32")
                                                                  : QStringLiteral("64"));
            if (!classifier.isEmpty()) {
                const QJsonObject cls =
                    dls.value(QStringLiteral("classifiers")).toObject().value(classifier).toObject();
                lib.isNative = true;
                lib.nativeClassifier = classifier;
                lib.path = cls.value(QStringLiteral("path")).toString();
                lib.url = cls.value(QStringLiteral("url")).toString();
                lib.sha1 = cls.value(QStringLiteral("sha1")).toString();
                lib.size = qint64(cls.value(QStringLiteral("size")).toDouble(-1));
            }
        }

        const QJsonArray exclude =
            lo.value(QStringLiteral("extract")).toObject().value(QStringLiteral("exclude")).toArray();
        for (const QJsonValue &e : exclude)
            lib.extractExclude << e.toString();

        if (!lib.name.isEmpty())
            v.libraries << lib;
    }

    return v;
}

QList<McManifestEntry> McVersion::parseManifest(const QJsonObject &root)
{
    QList<McManifestEntry> out;
    const QJsonArray versions = root.value(QStringLiteral("versions")).toArray();
    for (const QJsonValue &v : versions) {
        const QJsonObject o = v.toObject();
        McManifestEntry e;
        e.id = o.value(QStringLiteral("id")).toString();
        e.type = o.value(QStringLiteral("type")).toString();
        e.url = o.value(QStringLiteral("url")).toString();
        e.sha1 = o.value(QStringLiteral("sha1")).toString();
        e.releaseTime = o.value(QStringLiteral("releaseTime")).toString();
        if (!e.id.isEmpty())
            out << e;
    }
    return out;
}

McVersionInfo McVersion::mergeInherited(const McVersionInfo &child, const McVersionInfo &parent)
{
    McVersionInfo merged = parent; // 从父版本起手，再用子版本覆盖

    if (!child.id.isEmpty())
        merged.id = child.id;
    if (!child.type.isEmpty())
        merged.type = child.type;
    if (!child.mainClass.isEmpty())
        merged.mainClass = child.mainClass;
    if (!child.assets.isEmpty())
        merged.assets = child.assets;
    if (child.javaMajor > 0)
        merged.javaMajor = child.javaMajor;

    // 客户端 jar 与资源索引：子版本通常不提供，提供了才覆盖
    if (!child.clientUrl.isEmpty()) {
        merged.clientUrl = child.clientUrl;
        merged.clientSha1 = child.clientSha1;
        merged.clientSize = child.clientSize;
    }
    if (!child.assetIndex.url.isEmpty())
        merged.assetIndex = child.assetIndex;
    if (!child.assetIndex.id.isEmpty() && merged.assetIndex.id.isEmpty())
        merged.assetIndex.id = child.assetIndex.id;

    merged.jvmArguments = parent.jvmArguments + child.jvmArguments;
    merged.gameArguments = parent.gameArguments + child.gameArguments;

    // 库：父打底，子同名覆盖
    QList<McLibrary> libraries = parent.libraries;
    QHash<QString, int> byName;
    for (int i = 0; i < libraries.size(); ++i)
        byName.insert(libraries.at(i).name, i);

    for (const McLibrary &library : child.libraries) {
        const auto it = byName.constFind(library.name);
        if (it != byName.constEnd()) {
            libraries[it.value()] = library;
        } else {
            byName.insert(library.name, libraries.size());
            libraries << library;
        }
    }
    merged.libraries = libraries;

    // 继承关系已经消化掉了，别再带着往下走
    merged.inheritsFrom.clear();
    return merged;
}

QString McVersion::substitute(const QString &in, const QMap<QString, QString> &vars)
{
    QString out = in;
    for (auto it = vars.constBegin(); it != vars.constEnd(); ++it)
        out.replace(QStringLiteral("${") + it.key() + QLatin1Char('}'), it.value());
    return out;
}
