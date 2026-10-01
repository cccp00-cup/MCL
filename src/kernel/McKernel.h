#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

// 启动内核的抽象接口。
//
// 内核是**完全自研**的：自己读版本清单、并发下载并逐个校验 SHA1、解压 natives、
// 拼 java 命令行、拉起游戏进程，只依赖 Qt 和 zlib。
// （最早打算复用 PrismLauncher 的启动核心源码，调研后发现关键路径上离不开
// libarchive / tomlplusplus 这类依赖，装不上也裁不掉，于是改成自研。）
//
// 这一层只暴露界面真正需要的能力（枚举实例 / 启动 / 停止 / 日志），实现可替换：
//
//   VanillaKernel —— 当前实现，覆盖原版 / Fabric / Forge / NeoForge
//   StubKernel    —— 最早的占位实现，返回假数据，现在只留作对比调试
//                   （meta/ 版本元数据、java/ 的查找与校验、minecraft/ 的
//                    LaunchProfile / Library / Rule / natives、tasks/ 的步骤链）
//
// 关键约束：本头文件里不出现任何 Prism 的类型。内核实现怎么演进，界面层都不该改。
// 实例用 QVariantMap 传递，键为：
//   id / name / version / loader / dir / running
class McKernel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool available READ available NOTIFY availabilityChanged)
    Q_PROPERTY(QString unavailableReason READ unavailableReason NOTIFY availabilityChanged)
    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)

public:
    explicit McKernel(QObject *parent = nullptr) : QObject(parent) {}
    ~McKernel() override = default;

    virtual QString name() const = 0;
    virtual bool available() const { return m_available; }
    virtual QString unavailableReason() const { return m_reason; }
    QVariantList instances() const { return m_instances; }

    // 重新扫描实例目录
    Q_INVOKABLE virtual void refresh() = 0;

    // 启动实例；offline 变体用于尚未接入账户体系时先跑通链路
    Q_INVOKABLE virtual void launch(const QString &instanceId) = 0;
    Q_INVOKABLE virtual void launchOffline(const QString &instanceId, const QString &playerName) = 0;

    // 某个实例是否正在运行
    Q_INVOKABLE virtual bool isRunning(const QString &instanceId) const
    {
        Q_UNUSED(instanceId)
        return false;
    }

Q_SIGNALS:
    void instancesChanged();
    void availabilityChanged();
    void logLine(const QString &line);
    void launchStarted(const QString &instanceId);
    void launchFailed(const QString &instanceId, const QString &error);
    void launchFinished(const QString &instanceId, int exitCode);
    // 人话描述的当前阶段，如"正在下载库（128/431）"
    void stageChanged(const QString &stage);
    // 下载进度：已完成数 / 总数 / 已收字节 / 总字节
    void downloadProgress(int done, int total, qint64 bytes, qint64 bytesTotal);

protected:
    void setInstances(const QVariantList &list)
    {
        m_instances = list;
        Q_EMIT instancesChanged();
    }

    void setAvailability(bool available, const QString &reason = QString())
    {
        if (m_available == available && m_reason == reason)
            return;
        m_available = available;
        m_reason = reason;
        Q_EMIT availabilityChanged();
    }

    static QVariantMap makeInstance(const QString &id, const QString &name, const QString &version,
                                    const QString &loader, const QString &dir, bool running = false)
    {
        QVariantMap m;
        m[QStringLiteral("id")] = id;
        m[QStringLiteral("name")] = name;
        m[QStringLiteral("version")] = version;
        m[QStringLiteral("loader")] = loader;
        m[QStringLiteral("dir")] = dir;
        m[QStringLiteral("running")] = running;
        return m;
    }

private:
    QVariantList m_instances;
    bool m_available = false;
    QString m_reason;
};
