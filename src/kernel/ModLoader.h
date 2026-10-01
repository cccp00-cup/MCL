#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

// 模组加载器的"目录"信息：去哪查版本、去哪下 installer。
//
// 和 Fabric 的差别值得记一笔：
//   · Fabric 直接从 Meta 拿一份现成的 profile JSON（inheritsFrom 原版），我们合并就行。
//   · Forge / NeoForge 必须**跑 installer**：它们的安装涉及给客户端打补丁、生成
//     patched jar、注入一堆 processors，自己实现不现实。所以 mcl 的做法是
//     "下载 installer → 让 installer 装到共享数据目录 → 捡它生成的版本描述"。
namespace ModLoader
{
enum class Kind {
    Vanilla,
    Fabric,
    Forge,
    NeoForge,
};

Kind fromId(const QString &id);
QString displayName(Kind kind);

// 该加载器在 maven 上的元数据地址
QString metadataUrl(Kind kind);

// 从 maven-metadata.xml 里挑出适配 gameVersion 的加载器版本（返回最新的那个）。
// Forge 的版本形如 "1.20.1-47.4.9"，NeoForge 是 "20.1.100"。
QString pickVersion(Kind kind, const QByteArray &metadataXml, const QString &gameVersion);

// installer 的下载地址
QString installerUrl(Kind kind, const QString &gameVersion, const QString &loaderVersion);
} // namespace ModLoader
