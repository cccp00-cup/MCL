#include "DesktopController.h"
#include "kernel/InstanceStore.h"
#include "WindowModel.h"

#include <QDateTime>
#include <QLocale>
#include <QTimer>
#include <QVariantMap>

namespace
{
// 新窗口的基准尺寸与位置（桌面默认 1280x800 时视觉舒服）
constexpr qreal kDefaultW = 760.0;
constexpr qreal kDefaultH = 500.0;
constexpr qreal kBaseX = 150.0;
constexpr qreal kBaseY = 90.0;
constexpr qreal kCascadeStep = 28.0;
constexpr int kCascadeWrap = 6;

QString iconPath(const char *name)
{
    return QStringLiteral("qrc:/mcl/icons/%1.svg").arg(QLatin1String(name));
}
} // namespace

DesktopController::DesktopController(InstanceStore *store, QObject *parent)
    : QObject(parent)
    , m_windows(new WindowModel(this))
    , m_store(store)
{
    rebuildApps();
    if (m_store)
        connect(m_store, &InstanceStore::instancesChanged, this, &DesktopController::rebuildApps);
    updateClock();

    auto *timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &DesktopController::updateClock);
    timer->start();
}

void DesktopController::rebuildApps()
{
    auto make = [](const QString &id, const QString &name, const QString &icon,
                   const QString &accent, const QString &kind) {
        QVariantMap m;
        m[QStringLiteral("id")] = id;
        m[QStringLiteral("name")] = name;
        m[QStringLiteral("icon")] = icon;
        m[QStringLiteral("accent")] = accent;
        // kind 决定点/右键的行为：app = 开窗口，instance = 启动游戏
        m[QStringLiteral("kind")] = kind;
        return m;
    };

    m_apps = QVariantList{
        // 启动台放第一位 —— macOS 的 Dock 里它就在这儿
        make(QStringLiteral("launchpad"), QStringLiteral("启动台"), iconPath("launchpad"),
             QStringLiteral("#5c74c8"), QStringLiteral("app")),
        make(QStringLiteral("launcher"), QStringLiteral("启动器"), iconPath("cube"),
             QStringLiteral("#3d7df6"), QStringLiteral("app")),
        make(QStringLiteral("mods"), QStringLiteral("模组市场"), iconPath("package"),
             QStringLiteral("#6a4ec8"), QStringLiteral("app")),
        make(QStringLiteral("packs"), QStringLiteral("整合包市场"), iconPath("package"),
             QStringLiteral("#c26a3a"), QStringLiteral("app")),
        make(QStringLiteral("resources"), QStringLiteral("资源包市场"), iconPath("palette"),
             QStringLiteral("#4a9e8f"), QStringLiteral("app")),
        make(QStringLiteral("shaders"), QStringLiteral("光影市场"), iconPath("sparkle"),
             QStringLiteral("#8a5ec8"), QStringLiteral("app")),
        make(QStringLiteral("console"), QStringLiteral("控制台"), iconPath("terminal"),
             QStringLiteral("#2f3542"), QStringLiteral("app")),
        make(QStringLiteral("settings"), QStringLiteral("系统设置"), iconPath("gear"),
             QStringLiteral("#8b93a7"), QStringLiteral("app")),
        make(QStringLiteral("about"), QStringLiteral("关于本机"), iconPath("info"),
             QStringLiteral("#c0c6d4"), QStringLiteral("app")),
    };

    // 用户钉到 Dock 上的实例，紧跟在应用后面
    if (m_store) {
        const QVariantList pinned = m_store->pinnedInstances();
        for (const QVariant &value : pinned) {
            const QVariantMap instance = value.toMap();
            // 用实例自己的图标（按加载器区分），没有就给个兜底
            QString icon = instance.value(QStringLiteral("icon")).toString();
            if (icon.isEmpty())
                icon = iconPath("grass-block");
            m_apps << make(instance.value(QStringLiteral("id")).toString(),
                           instance.value(QStringLiteral("name")).toString(), icon,
                           QStringLiteral("#3f8f5f"), QStringLiteral("instance"));
        }
    }

    Q_EMIT appsChanged();
}

void DesktopController::updateClock()
{
    const QDateTime now = QDateTime::currentDateTime();
    const QLocale loc(QLocale::Chinese, QLocale::China);

    const QString clock = loc.toString(now.time(), QStringLiteral("HH:mm"));
    const QString date = loc.toString(now.date(), QStringLiteral("M月d日 ddd"));
    if (clock == m_clockText && date == m_dateText)
        return;

    m_clockText = clock;
    m_dateText = date;
    Q_EMIT clockChanged();
}

QStringList DesktopController::runningApps() const
{
    QStringList list(m_running.constBegin(), m_running.constEnd());
    list.sort();
    return list;
}

QString DesktopController::frontAppName() const
{
    if (m_frontAppId == QLatin1String("finder"))
        return QStringLiteral("访达");
    for (const QVariant &v : m_apps) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() == m_frontAppId)
            return m.value(QStringLiteral("name")).toString();
    }
    return QStringLiteral("访达");
}

QString DesktopController::frontAppId() const
{
    return m_frontAppId;
}

void DesktopController::setOverlay(const QString &overlay)
{
    if (m_overlay == overlay)
        return;
    m_overlay = overlay;
    Q_EMIT overlayChanged();
}

void DesktopController::setEditingInstance(const QString &instanceId)
{
    if (m_editingInstance == instanceId)
        return;
    m_editingInstance = instanceId;
    Q_EMIT editingInstanceChanged();
}

void DesktopController::notifyFrontApp()
{
    // 取 z 最高的窗口所属应用；没有窗口就是访达
    const WindowModel *model = m_windows;
    QString top;
    int topZ = -1;
    for (int row = 0; row < model->rowCount(); ++row) {
        const QVariant z = model->data(model->index(row, 0), WindowModel::ZRole);
        const QVariant min = model->data(model->index(row, 0), WindowModel::MinimizedRole);
        if (min.toBool())
            continue;
        if (z.toInt() > topZ) {
            topZ = z.toInt();
            top = model->data(model->index(row, 0), WindowModel::AppIdRole).toString();
        }
    }
    const QString next = top.isEmpty() ? QStringLiteral("finder") : top;
    if (next == m_frontAppId)
        return;
    m_frontAppId = next;
    Q_EMIT frontAppChanged();
}

QVariantMap DesktopController::appInfo(const QString &appId) const
{
    for (const QVariant &v : m_apps) {
        const QVariantMap m = v.toMap();
        if (m.value(QStringLiteral("id")).toString() == appId)
            return m;
    }
    return {};
}

bool DesktopController::isRunning(const QString &appId) const
{
    return m_running.contains(appId);
}

int DesktopController::windowCountForApp(const QString &appId) const
{
    return m_windows->countForApp(appId);
}

int DesktopController::focusedWindowId() const
{
    for (int row = 0; row < m_windows->rowCount(); ++row) {
        const QModelIndex idx = m_windows->index(row, 0);
        if (!m_windows->data(idx, WindowModel::FocusedRole).toBool())
            continue;
        if (m_windows->data(idx, WindowModel::MinimizedRole).toBool())
            continue;
        return m_windows->data(idx, WindowModel::WindowIdRole).toInt();
    }
    return 0;
}

void DesktopController::openApp(const QString &appId)
{
    if (appId.isEmpty() || appId == QLatin1String("finder"))
        return;

    // 「启动台」不是窗口应用，而是一层全屏覆盖：点 Dock 图标来回切
    if (appId == QLatin1String("launchpad")) {
        setOverlay(m_overlay == QLatin1String("launchpad") ? QString()
                                                          : QStringLiteral("launchpad"));
        return;
    }

    const QList<int> ids = m_windows->windowIdsForApp(appId);
    if (!ids.isEmpty()) {
        // 应用已在运行：把它的窗口逐个恢复并聚焦（macOS 的 Dock 点击行为）
        for (int id : ids) {
            m_windows->setMinimized(id, false);
            m_windows->raise(id);
        }
        notifyFrontApp();
        return;
    }

    const QVariantMap info = appInfo(appId);
    QString title = info.value(QStringLiteral("name")).toString();
    if (title.isEmpty())
        title = appId;

    const qreal step = kCascadeStep * (m_cascade % kCascadeWrap);
    ++m_cascade;
    const QRectF geometry(kBaseX + step, kBaseY + step, kDefaultW, kDefaultH);
    m_windows->addWindow(appId, title, geometry);

    if (!m_running.contains(appId)) {
        m_running.insert(appId);
        Q_EMIT runningAppsChanged();
    }
    notifyFrontApp();
}

void DesktopController::closeWindow(int windowId)
{
    // macOS 语义：关掉窗口不等于退出应用 —— 应用仍在 Dock 上亮着，再点一下会重开窗口
    m_windows->removeWindow(windowId);
    notifyFrontApp();
}

void DesktopController::quitApp(const QString &appId)
{
    const QList<int> ids = m_windows->windowIdsForApp(appId);
    for (int id : ids)
        m_windows->removeWindow(id);

    if (m_running.remove(appId))
        Q_EMIT runningAppsChanged();
    notifyFrontApp();
}

void DesktopController::focusWindow(int windowId)
{
    m_windows->raise(windowId);
    notifyFrontApp();
}

void DesktopController::toggleMinimize(int windowId)
{
    const WindowItem *item = m_windows->itemFor(windowId);
    if (!item)
        return;
    m_windows->setMinimized(windowId, !item->minimized);
    notifyFrontApp();
}

void DesktopController::toggleMaximize(int windowId, qreal workX, qreal workY, qreal workW,
                                       qreal workH)
{
    const WindowItem *item = m_windows->itemFor(windowId);
    if (!item)
        return;

    if (item->maximized) {
        m_windows->setMaximized(windowId, false);
    } else {
        m_windows->setMaximized(windowId, true);
        // 最大化时不压住菜单栏，也不被 Dock 遮住 —— 工作区由 QML 传进来
        m_windows->setGeometry(windowId, QRectF(workX, workY, workW, workH));
    }
    m_windows->raise(windowId);
    notifyFrontApp();
}

void DesktopController::setWindowGeometry(int windowId, qreal x, qreal y, qreal w, qreal h)
{
    m_windows->setGeometry(windowId, QRectF(x, y, w, h));
}
