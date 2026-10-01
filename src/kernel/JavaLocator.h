#pragma once

#include <QList>
#include <QString>

// 本机的一个 Java 运行时
struct JavaInstall
{
    QString path;    // java 可执行文件
    int major = 0;   // 主版本：8 / 17 / 21 ...
    QString version; // 完整版本串
    QString vendor;  // OpenJDK / Temurin / ...（仅用于展示）

    bool isValid() const { return !path.isEmpty() && major > 0; }
    QString label() const;
};

// 查找本机的 Java。Minecraft 对 Java 版本有硬性要求（version.json 里的
// javaVersion.majorVersion），所以这里必须能报出主版本号。
namespace JavaLocator
{
// 扫一遍常见安装位置（JAVA_HOME、PATH、/usr/lib/jvm、sdkman、jdks、macOS 的
// JavaVirtualMachines、Windows 的 Program Files）。结果按主版本降序。
QList<JavaInstall> findAll();

// 选一个最合适的：优先主版本正好等于 required，其次取比它大的里面最小的，
// 最后才退到比它小的里面最大的（这种情况下游戏很可能起不来）。
JavaInstall findBest(int requiredMajor);

// 唯一探测 java 版本的入口（probe 会真的跑一次 `java -version`）
JavaInstall probe(const QString &javaExecutable);
} // namespace JavaLocator
