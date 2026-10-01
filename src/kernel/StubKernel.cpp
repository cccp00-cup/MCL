#include "StubKernel.h"

#include <QTimer>

StubKernel::StubKernel(QObject *parent)
    : McKernel(parent)
{
    refresh();
}

QString StubKernel::name() const
{
    return QStringLiteral("占位内核");
}

QString StubKernel::unavailableReason() const
{
    return QStringLiteral("尚未接入 PrismLauncher 启动核心；当前显示的是示例数据。");
}

bool StubKernel::isRunning(const QString &instanceId) const
{
    return m_running.contains(instanceId);
}

void StubKernel::refresh()
{
    const QString home = QStringLiteral("~/minecraft/instances");
    setInstances(QVariantList{
        makeInstance(QStringLiteral("survival"), QStringLiteral("生存 1.20.1"),
                     QStringLiteral("1.20.1"), QStringLiteral("Fabric 0.15.7"),
                     home + QStringLiteral("/survival"), m_running.contains(QStringLiteral("survival"))),
        makeInstance(QStringLiteral("vanilla"), QStringLiteral("原版 1.21"),
                     QStringLiteral("1.21"), QStringLiteral("Vanilla"),
                     home + QStringLiteral("/vanilla"), m_running.contains(QStringLiteral("vanilla"))),
        makeInstance(QStringLiteral("create"), QStringLiteral("机械动力 1.19.2"),
                     QStringLiteral("1.19.2"), QStringLiteral("Forge 43.3.0"),
                     home + QStringLiteral("/create"), m_running.contains(QStringLiteral("create"))),
    });
    setAvailability(false, unavailableReason());
}

void StubKernel::launch(const QString &instanceId)
{
    launchOffline(instanceId, QStringLiteral("Player"));
}

void StubKernel::launchOffline(const QString &instanceId, const QString &playerName)
{
    Q_EMIT logLine(QStringLiteral("[mcl] 启动请求：%1（玩家 %2）").arg(instanceId, playerName));
    Q_EMIT launchStarted(instanceId);

    const QString reason = unavailableReason();
    QTimer::singleShot(0, this, [this, instanceId, reason]() {
        Q_EMIT logLine(QStringLiteral("[mcl] 内核未接入，已中止。"));
        Q_EMIT launchFailed(instanceId, reason);
    });
}
