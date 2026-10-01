#include "InstanceStore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

InstanceStore::InstanceStore(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(rootDir());
    load();
}

QString InstanceStore::storePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/instances.json");
}

QString InstanceStore::rootDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/instances");
}

QString InstanceStore::defaultIcon(const QString &loader)
{
    const QString lower = loader.toLower();
    if (lower == QLatin1String("fabric"))
        return QStringLiteral("qrc:/mcl/icons/fabric.svg");
    if (lower == QLatin1String("forge"))
        return QStringLiteral("qrc:/mcl/icons/anvil.svg");
    if (lower == QLatin1String("neoforge"))
        return QStringLiteral("qrc:/mcl/icons/fox.svg");
    return QStringLiteral("qrc:/mcl/icons/grass-block.svg"); // 原版：草方块
}

QString InstanceStore::instanceDir(const QString &id) const
{
    return rootDir() + QLatin1Char('/') + id;
}

int InstanceStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries.at(i).id == id)
            return i;
    return -1;
}

QVariantList InstanceStore::instances() const
{
    QVariantList list;
    for (const Entry &entry : m_entries) {
        QVariantMap item;
        item[QStringLiteral("id")] = entry.id;
        item[QStringLiteral("name")] = entry.name;
        item[QStringLiteral("versionId")] = entry.versionId;
        item[QStringLiteral("version")] = entry.versionId;
        item[QStringLiteral("loader")] = entry.loader;
        item[QStringLiteral("icon")] = entry.icon.isEmpty() ? defaultIcon(entry.loader) : entry.icon;
        item[QStringLiteral("javaPath")] = entry.javaPath;
        item[QStringLiteral("dir")] = instanceDir(entry.id);
        item[QStringLiteral("pinned")] = entry.pinned;
        list << item;
    }
    return list;
}

QVariantList InstanceStore::pinnedInstances() const
{
    QVariantList list;
    for (const Entry &entry : m_entries) {
        if (!entry.pinned)
            continue;
        QVariantMap item;
        item[QStringLiteral("id")] = entry.id;
        item[QStringLiteral("name")] = entry.name;
        item[QStringLiteral("versionId")] = entry.versionId;
        item[QStringLiteral("loader")] = entry.loader;
        item[QStringLiteral("icon")] = entry.icon.isEmpty() ? defaultIcon(entry.loader) : entry.icon;
        list << item;
    }
    return list;
}

QVariantMap InstanceStore::instance(const QString &id) const
{
    const int index = indexOf(id);
    if (index < 0)
        return {};
    const Entry &entry = m_entries.at(index);
    QVariantMap item;
    item[QStringLiteral("id")] = entry.id;
    item[QStringLiteral("name")] = entry.name;
    item[QStringLiteral("versionId")] = entry.versionId;
    item[QStringLiteral("loader")] = entry.loader;
    item[QStringLiteral("icon")] = entry.icon.isEmpty() ? defaultIcon(entry.loader) : entry.icon;
    item[QStringLiteral("javaPath")] = entry.javaPath;
    item[QStringLiteral("dir")] = instanceDir(entry.id);
    item[QStringLiteral("pinned")] = entry.pinned;
    return item;
}

QString InstanceStore::create(const QString &name, const QString &versionId, const QString &loader,
                              const QString &icon)
{
    if (versionId.isEmpty())
        return {};

    Entry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    entry.versionId = versionId;
    entry.loader = loader.isEmpty() ? QStringLiteral("Vanilla") : loader;
    entry.icon = icon.isEmpty() ? defaultIcon(entry.loader) : icon;
    entry.name = name.isEmpty()
        ? QStringLiteral("%1 · %2").arg(entry.loader, entry.versionId)
        : name;

    m_entries.append(entry);

    // 每个实例有自己的游戏目录（saves/mods/config 都在这儿）
    QDir().mkpath(instanceDir(entry.id));

    save();
    Q_EMIT instancesChanged();
    return entry.id;
}

void InstanceStore::rename(const QString &id, const QString &name)
{
    const int index = indexOf(id);
    if (index < 0 || name.isEmpty())
        return;
    if (m_entries.at(index).name == name)
        return;
    m_entries[index].name = name;
    save();
    Q_EMIT instancesChanged();
}

void InstanceStore::remove(const QString &id)
{
    const int index = indexOf(id);
    if (index < 0)
        return;

    // 只把实例从清单里摘掉；它的游戏目录留着不动 —— 删存档是不可逆的，
    // 让用户自己在文件管理器里处理更安全。
    m_entries.removeAt(index);
    save();
    Q_EMIT instancesChanged();
}

void InstanceStore::move(const QString &id, int newIndex)
{
    const int index = indexOf(id);
    if (index < 0)
        return;

    newIndex = qBound(0, newIndex, int(m_entries.size()) - 1);
    if (index == newIndex)
        return;

    m_entries.move(index, newIndex);
    save();
    Q_EMIT instancesChanged();
}

void InstanceStore::setJava(const QString &id, const QString &javaPath)
{
    const int index = indexOf(id);
    if (index < 0 || m_entries.at(index).javaPath == javaPath)
        return;
    m_entries[index].javaPath = javaPath;
    save();
    Q_EMIT instancesChanged();
}

void InstanceStore::setPinned(const QString &id, bool pinned)
{
    const int index = indexOf(id);
    if (index < 0 || m_entries.at(index).pinned == pinned)
        return;
    m_entries[index].pinned = pinned;
    save();
    Q_EMIT instancesChanged();
}

void InstanceStore::load()
{
    QFile file(storePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonArray array =
        QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("instances")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        Entry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.name = object.value(QStringLiteral("name")).toString();
        entry.versionId = object.value(QStringLiteral("versionId")).toString();
        entry.loader = object.value(QStringLiteral("loader")).toString(QStringLiteral("Vanilla"));
        entry.icon = object.value(QStringLiteral("icon")).toString();
        entry.javaPath = object.value(QStringLiteral("javaPath")).toString();
        entry.pinned = object.value(QStringLiteral("pinned")).toBool(false);
        if (!entry.id.isEmpty() && !entry.versionId.isEmpty())
            m_entries.append(entry);
    }
}

void InstanceStore::save() const
{
    QJsonArray array;
    for (const Entry &entry : m_entries) {
        QJsonObject object;
        object[QStringLiteral("id")] = entry.id;
        object[QStringLiteral("name")] = entry.name;
        object[QStringLiteral("versionId")] = entry.versionId;
        object[QStringLiteral("loader")] = entry.loader;
        object[QStringLiteral("icon")] = entry.icon;
        if (!entry.javaPath.isEmpty())
            object[QStringLiteral("javaPath")] = entry.javaPath;
        object[QStringLiteral("pinned")] = entry.pinned;
        array.append(object);
    }

    QJsonObject root;
    root[QStringLiteral("instances")] = array;

    QFile file(storePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}
