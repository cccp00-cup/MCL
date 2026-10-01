#include "WindowModel.h"

WindowModel::WindowModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int WindowModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_windows.size());
}

QVariant WindowModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_windows.size())
        return {};

    const WindowItem &w = m_windows.at(index.row());
    switch (role) {
    case WindowIdRole:  return w.id;
    case AppIdRole:     return w.appId;
    case TitleRole:     return w.title;
    case XRole:         return w.geometry.x();
    case YRole:         return w.geometry.y();
    case WidthRole:     return w.geometry.width();
    case HeightRole:    return w.geometry.height();
    case MinimizedRole: return w.minimized;
    case MaximizedRole: return w.maximized;
    case ZRole:         return w.z;
    case FocusedRole:   return w.focused;
    default:            return {};
    }
}

QHash<int, QByteArray> WindowModel::roleNames() const
{
    return {
        { WindowIdRole,  QByteArrayLiteral("windowId") },
        { AppIdRole,     QByteArrayLiteral("appId") },
        { TitleRole,     QByteArrayLiteral("title") },
        { XRole,         QByteArrayLiteral("winX") },
        { YRole,         QByteArrayLiteral("winY") },
        { WidthRole,     QByteArrayLiteral("winWidth") },
        { HeightRole,    QByteArrayLiteral("winHeight") },
        { MinimizedRole, QByteArrayLiteral("minimized") },
        { MaximizedRole, QByteArrayLiteral("maximized") },
        { ZRole,         QByteArrayLiteral("stackZ") },
        { FocusedRole,   QByteArrayLiteral("focused") },
    };
}

void WindowModel::touch(int row, const QVector<int> &roles)
{
    if (row < 0 || row >= m_windows.size())
        return;
    const QModelIndex idx = index(row, 0);
    Q_EMIT dataChanged(idx, idx, roles);
}

int WindowModel::addWindow(const QString &appId, const QString &title, const QRectF &geometry)
{
    WindowItem w;
    w.id = m_nextId++;
    w.appId = appId;
    w.title = title;
    w.geometry = geometry;
    w.restoreGeometry = geometry;
    w.z = m_nextZ++;

    const int row = int(m_windows.size());
    beginInsertRows(QModelIndex(), row, row);
    m_windows.append(w);
    endInsertRows();
    raise(w.id);
    return w.id;
}

void WindowModel::removeWindow(int windowId)
{
    const int row = rowForWindow(windowId);
    if (row < 0)
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_windows.removeAt(row);
    endRemoveRows();
}

void WindowModel::setGeometry(int windowId, const QRectF &geometry)
{
    const int row = rowForWindow(windowId);
    if (row < 0)
        return;
    WindowItem &w = m_windows[row];
    if (w.geometry == geometry)
        return;
    w.geometry = geometry;
    if (!w.maximized)
        w.restoreGeometry = geometry;
    touch(row, { XRole, YRole, WidthRole, HeightRole });
}

void WindowModel::setMinimized(int windowId, bool minimized)
{
    const int row = rowForWindow(windowId);
    if (row < 0 || m_windows.at(row).minimized == minimized)
        return;
    m_windows[row].minimized = minimized;
    touch(row, { MinimizedRole });
}

void WindowModel::setMaximized(int windowId, bool maximized)
{
    const int row = rowForWindow(windowId);
    if (row < 0 || m_windows.at(row).maximized == maximized)
        return;

    WindowItem &w = m_windows[row];
    w.maximized = maximized;
    if (maximized) {
        w.restoreGeometry = w.geometry;
        // 由 QML 侧在最大化时把几何设成桌面工作区，这里只翻标志位
    } else if (w.restoreGeometry.isValid()) {
        w.geometry = w.restoreGeometry;
    }
    touch(row, { MaximizedRole, XRole, YRole, WidthRole, HeightRole });
}

void WindowModel::raise(int windowId)
{
    const int row = rowForWindow(windowId);
    if (row < 0)
        return;

    const int z = m_nextZ++;
    const bool wasFocused = m_windows.at(row).focused;

    for (int i = 0; i < m_windows.size(); ++i) {
        bool changed = false;
        if (i == row) {
            if (m_windows[i].z != z) { m_windows[i].z = z; changed = true; }
            if (!m_windows[i].focused) { m_windows[i].focused = true; changed = true; }
        } else if (m_windows[i].focused) {
            m_windows[i].focused = false;
            changed = true;
        }
        if (changed)
            touch(i, { ZRole, FocusedRole });
    }

    Q_UNUSED(wasFocused)
}

int WindowModel::rowForWindow(int windowId) const
{
    for (int i = 0; i < m_windows.size(); ++i)
        if (m_windows.at(i).id == windowId)
            return i;
    return -1;
}

const WindowItem *WindowModel::itemFor(int windowId) const
{
    const int row = rowForWindow(windowId);
    return row < 0 ? nullptr : &m_windows.at(row);
}

QList<int> WindowModel::windowIdsForApp(const QString &appId) const
{
    QList<int> ids;
    for (const WindowItem &w : m_windows)
        if (w.appId == appId)
            ids << w.id;
    return ids;
}

int WindowModel::topZ() const
{
    int z = 0;
    for (const WindowItem &w : m_windows)
        z = qMax(z, w.z);
    return z;
}
