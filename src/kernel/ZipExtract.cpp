#include "ZipExtract.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <zlib.h>

namespace
{
quint16 readU16(const QByteArray &d, int offset)
{
    return quint16(quint8(d.at(offset))) | (quint16(quint8(d.at(offset + 1))) << 8);
}

quint32 readU32(const QByteArray &d, int offset)
{
    return quint32(quint8(d.at(offset))) | (quint32(quint8(d.at(offset + 1))) << 8)
        | (quint32(quint8(d.at(offset + 2))) << 16) | (quint32(quint8(d.at(offset + 3))) << 24);
}

// raw deflate（zip 里不带 zlib 头，所以 windowBits 取负）
QByteArray inflateRaw(const QByteArray &input, quint32 expectedSize)
{
    if (expectedSize == 0)
        return {};

    QByteArray out(int(expectedSize), Qt::Uninitialized);
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
        return {};

    zs.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(input.constData()));
    zs.avail_in = uInt(input.size());
    zs.next_out = reinterpret_cast<Bytef *>(out.data());
    zs.avail_out = uInt(out.size());

    const int rc = inflate(&zs, Z_FINISH);
    const int produced = int(zs.total_out);
    inflateEnd(&zs);

    if (rc != Z_STREAM_END && rc != Z_OK)
        return {};
    out.resize(produced);
    return out;
}

// 防御 zip slip：条目名里的 .. 一律拒掉
bool isSafeEntryName(const QString &name)
{
    if (name.isEmpty())
        return false;
    if (name.contains(QLatin1String("..")))
        return false;
    if (name.startsWith(QLatin1Char('/')) || name.startsWith(QLatin1Char('\\')))
        return false;
    return true;
}
} // namespace

bool ZipExtract::extract(const QString &zipPath, const QString &destDir,
                         const QStringList &excludePrefixes, QString *error)
{
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("打不开 %1：%2").arg(zipPath, file.errorString());
        return false;
    }
    // natives 包都很小（几十 KB ~ 几 MB），整体读进内存最省事
    const QByteArray data = file.readAll();
    file.close();

    if (data.size() < 22) {
        if (error)
            *error = QStringLiteral("%1 不是有效的 zip（太短）").arg(zipPath);
        return false;
    }

    // —— 从尾部找 End of Central Directory（注释最长 65535）
    int eocd = -1;
    const int lowest = qMax(0, int(data.size()) - 22 - 65535);
    for (int i = int(data.size()) - 22; i >= lowest; --i) {
        if (quint8(data.at(i)) == 0x50 && quint8(data.at(i + 1)) == 0x4b
            && quint8(data.at(i + 2)) == 0x05 && quint8(data.at(i + 3)) == 0x06) {
            eocd = i;
            break;
        }
    }
    if (eocd < 0) {
        if (error)
            *error = QStringLiteral("%1 找不到 zip 结尾标记").arg(zipPath);
        return false;
    }

    const int entryCount = readU16(data, eocd + 10);
    int cursor = int(readU32(data, eocd + 16)); // central directory 起始偏移

    QDir().mkpath(destDir);
    int extracted = 0;

    for (int i = 0; i < entryCount; ++i) {
        if (cursor + 46 > data.size())
            break;
        if (readU32(data, cursor) != 0x02014b50) // central directory header 签名
            break;

        const quint16 method = readU16(data, cursor + 10);
        const quint32 compSize = readU32(data, cursor + 20);
        const quint32 uncompSize = readU32(data, cursor + 24);
        const quint16 nameLen = readU16(data, cursor + 28);
        const quint16 extraLen = readU16(data, cursor + 30);
        const quint16 commentLen = readU16(data, cursor + 32);
        const quint32 localOffset = readU32(data, cursor + 42);

        if (cursor + 46 + nameLen > data.size())
            break;
        const QString name = QString::fromUtf8(data.mid(cursor + 46, nameLen));
        cursor += 46 + nameLen + extraLen + commentLen;

        if (name.endsWith(QLatin1Char('/'))) // 目录条目，按需建目录即可
            continue;

        bool excluded = false;
        for (const QString &prefix : excludePrefixes) {
            if (!prefix.isEmpty() && name.startsWith(prefix)) {
                excluded = true;
                break;
            }
        }
        if (excluded || !isSafeEntryName(name))
            continue;

        if (localOffset + 30 > quint32(data.size()))
            continue;
        const quint16 localNameLen = readU16(data, int(localOffset) + 26);
        const quint16 localExtraLen = readU16(data, int(localOffset) + 28);
        const int dataStart = int(localOffset) + 30 + localNameLen + localExtraLen;
        if (dataStart < 0 || dataStart + int(compSize) > data.size())
            continue;

        QByteArray content;
        if (method == 0) {
            content = data.mid(dataStart, int(compSize));
        } else if (method == 8) {
            content = inflateRaw(data.mid(dataStart, int(compSize)), uncompSize);
            if (content.isEmpty() && uncompSize != 0)
                continue;
        } else {
            continue; // 其它压缩方式不该出现在 natives 里
        }

        // 注意用 '/' 拼路径（Qt 自己会处理平台差异），条目名统一是正斜杠
        const QString target = destDir + QLatin1Char('/') + name;
        QDir().mkpath(QFileInfo(target).absolutePath());

        QFile out(target);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            if (error)
                *error = QStringLiteral("写不出 %1：%2").arg(target, out.errorString());
            return false;
        }
        out.write(content);
        out.close();

        // natives 里的 .jnilib 是旧 macOS 命名，实际是 dylib
        if (target.endsWith(QLatin1String(".jnilib"))) {
            QString renamed = target;
            renamed.chop(7); // ".jnilib"
            renamed += QLatin1String(".dylib");
            QFile::remove(renamed);
            QFile::rename(target, renamed);
        }

        ++extracted;
    }

    if (extracted == 0) {
        if (error)
            *error = QStringLiteral("%1 里没有可解压的条目").arg(zipPath);
        return false;
    }
    return true;
}
