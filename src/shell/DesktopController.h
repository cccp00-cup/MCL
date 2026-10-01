#pragma once

#include <QObject>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QVariantList>

#include "WindowModel.h"   // moc 需要 WindowModel 的完整定义（Q_PROPERTY 是 WindowModel*）

class InstanceStore;
class QTimer;

// 桌面控制器：菜单栏时钟、Dock 应用注册表、窗口生命周期。
//
// 这是一个纯粹的"状态机"，不碰任何界面。QML 只负责把它画出来、把鼠标手势翻译成
// 这里的调用 —— 这样将来加"上次退出时的桌面状态恢复""多桌面 Spaces"都只改这一层。
class DesktopController : public QObject
{
    Q_OBJECT

    // 菜单栏右侧：macOS 风格 "周三 9月30日 20:32"
    Q_PROPERTY(QString clockText READ clockText NOTIFY clockChanged)
    Q_PROPERTY(QString dateText READ dateText NOTIFY clockChanged)

    // Dock 里的条目：固定应用 + 用户钉上去的实例
    Q_PROPERTY(QVariantList apps READ apps NOTIFY appsChanged)

    // 窗口列表模型，直接给 Repeater 当 model
    Q_PROPERTY(WindowModel *windows READ windows CONSTANT)

    // 正在运行的应用 id：Dock 用它画图标下方的运行指示点
    Q_PROPERTY(QStringList runningApps READ runningApps NOTIFY runningAppsChanged)

    // 菜单栏左侧显示的名字：有窗口聚焦时是应用名，否则是"访达"
    Q_PROPERTY(QString frontAppName READ frontAppName NOTIFY frontAppChanged)
    Q_PROPERTY(QString frontAppId READ frontAppId NOTIFY frontAppChanged)

    // 桌面是否处于"进入 Launchpad / 显示桌面"这类全屏覆盖态；QML 用它做过渡
    Q_PROPERTY(QString overlay READ overlay WRITE setOverlay NOTIFY overlayChanged)

    // 「实例设置」窗口正在编辑哪个实例（打开这个窗口前由调用方设好）
    Q_PROPERTY(QString editingInstance READ editingInstance WRITE setEditingInstance
                   NOTIFY editingInstanceChanged)

public:
    explicit DesktopController(InstanceStore *store, QObject *parent = nullptr);

    QString clockText() const { return m_clockText; }
    QString dateText() const { return m_dateText; }
    QVariantList apps() const { return m_apps; }
    WindowModel *windows() const { return m_windows; }
    QStringList runningApps() const;
    QString frontAppName() const;
    QString frontAppId() const;
    QString overlay() const { return m_overlay; }
    QString editingInstance() const { return m_editingInstance; }

    void setOverlay(const QString &overlay);
    void setEditingInstance(const QString &instanceId);

    // —— 由 QML 调用的桌面操作 ——
    Q_INVOKABLE void openApp(const QString &appId);
    Q_INVOKABLE void closeWindow(int windowId);
    Q_INVOKABLE void quitApp(const QString &appId);
    Q_INVOKABLE void focusWindow(int windowId);
    Q_INVOKABLE void toggleMinimize(int windowId);
    Q_INVOKABLE void toggleMaximize(int windowId, qreal workX, qreal workY, qreal workW, qreal workH);
    Q_INVOKABLE void setWindowGeometry(int windowId, qreal x, qreal y, qreal w, qreal h);

    Q_INVOKABLE bool isRunning(const QString &appId) const;
    Q_INVOKABLE QVariantMap appInfo(const QString &appId) const;
    Q_INVOKABLE int windowCountForApp(const QString &appId) const;
    // 当前聚焦窗口的 id（没有窗口时返回 0），菜单栏的"关闭/最小化"用它
    Q_INVOKABLE int focusedWindowId() const;

Q_SIGNALS:
    void appsChanged();
    void clockChanged();
    void runningAppsChanged();
    void frontAppChanged();
    void overlayChanged();
    void editingInstanceChanged();

private:
    void buildAppRegistry();
    void rebuildApps();
    void updateClock();
    void notifyFrontApp();

    InstanceStore *m_store = nullptr;

    QString m_clockText;
    QString m_dateText;
    QVariantList m_apps;
    WindowModel *m_windows = nullptr;
    QSet<QString> m_running;
    QString m_frontAppId = QStringLiteral("finder");
    QString m_overlay;
    QString m_editingInstance;
    // 新窗口的级联偏移，避免每个窗口都精确重叠
    int m_cascade = 0;
};
