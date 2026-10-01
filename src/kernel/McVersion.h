#pragma once

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>

// Minecraft 启动相关的基础数据结构 —— 直接对应 Mojang 官方 version.json 的字段。
// 这一层刻意保持"纯数据 + 纯函数"：没有网络、没有文件、没有 Qt GUI，
// 因此可以单独写单元测试，也是整个内核里最容易出错、最需要可验证的部分。
//
// 参考：https://minecraft.wiki/w/Client.json

// —— rules 数组里的一条
//
// 求值语义（和官方客户端保持一致）：
//   · 从前往后逐条比对当前平台/特性；
//   · 最后一条**匹配**的规则决定结果（allow → 适用，disallow → 不适用）；
//   · **一条都不匹配时**，库默认适用、参数默认不适用 —— 两者默认值不同，别搞混。
struct McRule
{
    bool allow = true;
    QString osName;      // "windows" / "linux" / "osx"；空表示不限平台
    QString osArch;      // "x86" / "x86_64" / "arm64"
    QString osVersion;   // 正则，匹配 os.version
    QMap<QString, bool> features; // 如 is_demo_user / has_custom_resolution

    static QList<McRule> parse(const QJsonArray &array);
    bool matches(const QMap<QString, bool> &activeFeatures) const;
    // defaultValue 是"一条都没匹配上"时的结果
    static bool evaluate(const QList<McRule> &rules, const QMap<QString, bool> &features,
                         bool defaultValue);
};

// —— 命令行参数：可以是纯字符串，也可以带 rules 的对象
struct McArgument
{
    QList<McRule> rules;
    QStringList values;
};

// —— 库
struct McLibrary
{
    QString name;            // "com.mojang:brigadier:1.0.18"
    QString path;            // "com/mojang/brigadier/1.0.18/brigadier-1.0.18.jar"（相对 libraries/）
    QString url;             // 空 = 本地文件，不需要下载
    QString sha1;
    qint64 size = -1;

    bool isNative = false;             // 是不是 natives 包
    QString nativeClassifier;          // "natives-linux" 等，最终名称
    QStringList extractExclude;        // 解压 natives 时要排除的前缀（通常是 META-INF/）

    QList<McRule> rules;

    bool appliesTo(const QMap<QString, bool> &features) const;
};

// —— asset index
struct McAssetIndex
{
    QString id;
    QString url;
    QString sha1;
    qint64 size = -1;
    qint64 totalSize = -1;   // 全部对象的字节数，用于给进度估个上限
};

// —— 一个完整版本的元数据
struct McVersionInfo
{
    QString id;
    QString type;              // release / snapshot / old_beta ...
    QString mainClass;
    QString assets;            // asset index 的 id
    QString inheritsFrom;      // 一般为空（mcl 不做整合包继承）

    // 客户端 jar
    QString clientUrl;
    QString clientSha1;
    qint64 clientSize = -1;

    McAssetIndex assetIndex;

    // Java 要求（1.13+ 才有 javaVersion）
    int javaMajor = 8;

    // 启动参数
    QList<McArgument> jvmArguments;
    QList<McArgument> gameArguments;

    // 库
    QList<McLibrary> libraries;

    bool isValid() const { return !id.isEmpty() && !mainClass.isEmpty(); }
};

// 版本清单里的一条（用于列表展示与下载 version.json）
struct McManifestEntry
{
    QString id;
    QString type;
    QString url;
    QString sha1;
    QString releaseTime;
};

namespace McVersion {

// 解析一个 version.json（已解析成 QJsonObject）
McVersionInfo parseVersion(const QJsonObject &root);

// 解析版本清单 version_manifest_v2.json
QList<McManifestEntry> parseManifest(const QJsonObject &root);

// 当前平台在 rules 里用的名字（windows / linux / osx）
QString currentOsName();
// 当前平台在 natives 里用的分类名（linux / windows / osx）
QString currentNativesName();

// 默认的平台特性集合（目前只需要保证 os/arch 走对分支）
QMap<QString, bool> defaultFeatures();

// 把 ${...} 占位符替换成实际值
QString substitute(const QString &in, const QMap<QString, QString> &vars);

// 版本继承：模组加载器（Fabric / Forge / Quilt）给出的 version.json 都是
// "inheritsFrom 原版" 的薄壳，只写自己那部分。启动前必须把两者合成一份完整的。
//
// 合并规则（与官方启动器一致）：
//   · mainClass / assets / javaVersion 等：子版本优先
//   · 客户端 jar：子版本一般没有，沿用父版本
//   · 库：父版本打底，子版本同名的覆盖（Maven 坐标相同即视为同一个库）
//   · 参数：父在前、子在后（后者可以覆盖前者）
McVersionInfo mergeInherited(const McVersionInfo &child, const McVersionInfo &parent);

} // namespace McVersion
