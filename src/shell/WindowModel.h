#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QRectF>
#include <QString>

// 一个"桌面窗口"的状态。mcl 的窗口不是操作系统的真窗口，而是桌面内部自绘的矩形 ——
// 因此位置、层级、最小化状态全都由这里持有，将来可以直接序列化实现"重开恢复上次布局"。
struct WindowItem
{
    int id = 0;
    QString appId;
    QString title;
    QRectF geometry;      // 普通状态下的几何（最大化时另存）
    QRectF restoreGeometry; // 最大化前的几何
    bool minimized = false;
    bool maximized = false;
    int z = 0;
    bool focused = false;
};

// 窗口列表模型：给 QML 的 Repeater 当 model，只对变化的行发 dataChanged，
// 不会整体重建 delegate —— 拖拽窗口时才不会被打断。
class WindowModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        WindowIdRole = Qt::UserRole + 1,
        AppIdRole,
        TitleRole,
        XRole,
        YRole,
        WidthRole,
        HeightRole,
        MinimizedRole,
        MaximizedRole,
        ZRole,
        FocusedRole,
    };
    Q_ENUM(Role)

    explicit WindowModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int addWindow(const QString &appId, const QString &title, const QRectF &geometry);
    void removeWindow(int windowId);
    void setGeometry(int windowId, const QRectF &geometry);
    void setMinimized(int windowId, bool minimized);
    void setMaximized(int windowId, bool maximized);
    void raise(int windowId);

    int rowForWindow(int windowId) const;
    const WindowItem *itemFor(int windowId) const;
    QList<int> windowIdsForApp(const QString &appId) const;
    int countForApp(const QString &appId) const { return int(windowIdsForApp(appId).size()); }
    int topZ() const;

private:
    void touch(int row, const QVector<int> &roles);

    QList<WindowItem> m_windows;
    int m_nextId = 1;
    int m_nextZ = 1;
};
