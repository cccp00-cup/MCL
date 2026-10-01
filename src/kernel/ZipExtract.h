#pragma once

#include <QString>
#include <QStringList>

// 最小 zip 读取器：只做"把 zip 里的文件解到目录"这一件事。
//
// 为什么自己写：natives 包是 zip，而唯一成熟的选择 libarchive 在这台机器上没装。
// zip 的读取只需要 central directory + local header 两段结构 + raw deflate 解压，
// 用 Qt 自带的 zlib 就能完成，不值得为此引入一个系统库依赖。
//
// 只支持 stored(0) 与 deflate(8) —— 这两种覆盖了全部 Minecraft natives 包。
namespace ZipExtract
{
// 把 zipPath 里的文件解压到 destDir。
// excludePrefixes 对应 version.json 里 libraries[].extract.exclude（通常是 "META-INF/"）。
bool extract(const QString &zipPath, const QString &destDir, const QStringList &excludePrefixes,
             QString *error);
} // namespace ZipExtract
