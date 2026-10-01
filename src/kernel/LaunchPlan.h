#pragma once

#include "Downloader.h"
#include "McVersion.h"

#include <QString>
#include <QStringList>

// 启动前的"算账"部分：要下哪些文件、classpath 是什么、命令行怎么拼。
//
// 全部是纯函数（输入版本元数据 + 目录，输出清单/字符串），不碰网络、不碰进程 ——
// 内核里最容易出错的就是这几步，独立出来才能单独验证。
namespace LaunchPlan
{

// 版本本体需要下载的文件：客户端 jar + 适用平台的库 + asset index
QList<DownloadItem> collectVersionFiles(const McVersionInfo &version,
                                        const QString &versionsDir,
                                        const QString &librariesDir,
                                        const QString &assetsDir);

// 读 asset index，列出全部资源对象（数量上千，通常几百 MB）
QList<DownloadItem> collectAssets(const QString &indexPath, const QString &assetsDir);

// 离线账户的 UUID：Mojang 的规则是 "OfflinePlayer:<名字>" 的 MD5，再按 v3 改版本位
QString offlineUuid(const QString &playerName);

// classpath：客户端 jar 在前，然后是全部适用平台的库
QStringList classpathEntries(const McVersionInfo &version, const QString &clientJar,
                             const QString &librariesDir);

// 拼出完整的 java 参数（JVM 参数 + 主类 + 游戏参数）
QStringList buildArguments(const McVersionInfo &version,
                           const QString &playerName,
                           const QString &playerUuid,
                           const QString &accessToken,
                           const QString &userType,
                           const QString &gameDir,
                           const QString &assetsDir,
                           const QString &nativesDir,
                           const QString &librariesDir,
                           const QString &clientJar,
                           int memoryMb);

} // namespace LaunchPlan
