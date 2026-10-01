#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

// 实例仓库：mcl 里"一个实例"= 名字 + 版本 + 模组加载器 + 自己的游戏目录。
//
// 和"版本"的区别很重要：版本是 Mojang 那边的一个 json（1.20.1、1.21…），
// 实例是用户自己的东西 —— 可以有好几个实例跑同一个版本，各自有独立的
// saves / mods / 配置。启动台里显示、能改名、能拖拽、能钉到 Dock 的都是实例。
//
// 落盘：`~/.local/share/mcl/instances.json`（数组顺序就是启动台里的顺序）。
// 实例的游戏目录：`~/.local/share/mcl/instances/<id>/`。
class InstanceStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList instances READ instances NOTIFY instancesChanged)
    Q_PROPERTY(QVariantList pinnedInstances READ pinnedInstances NOTIFY instancesChanged)

public:
    explicit InstanceStore(QObject *parent = nullptr);

    QVariantList instances() const;
    // 被钉到 Dock 上的那些（并且保持启动台里的顺序）
    QVariantList pinnedInstances() const;

    static QString storePath();
    static QString rootDir();

    Q_INVOKABLE QVariantMap instance(const QString &id) const;
    Q_INVOKABLE QString instanceDir(const QString &id) const;

    // 加载器对应的默认图标（原版草方块 / Forge 铁砧 / NeoForge 狐狸 / Fabric 布料）
    static QString defaultIcon(const QString &loader);

    // 建一个新实例；返回它的 id（失败返回空串）。
    // icon 留空则按加载器取默认图；整合包会传它自己在 Modrinth 上的图标地址。
    Q_INVOKABLE QString create(const QString &name, const QString &versionId,
                               const QString &loader = QStringLiteral("Vanilla"),
                               const QString &icon = QString());
    Q_INVOKABLE void rename(const QString &id, const QString &name);
    Q_INVOKABLE void remove(const QString &id);
    // 把实例挪到第 newIndex 位（启动台拖拽排序）
    Q_INVOKABLE void move(const QString &id, int newIndex);
    // 钉到 Dock / 取消钉
    Q_INVOKABLE void setPinned(const QString &id, bool pinned);
    // 指定这个实例用哪个 Java（传空串 = 交回自动挑选）
    Q_INVOKABLE void setJava(const QString &id, const QString &javaPath);

Q_SIGNALS:
    void instancesChanged();

private:
    struct Entry
    {
        QString id;
        QString name;
        QString versionId;
        QString loader;
        QString icon;
        // 玩家自己指定的 Java；空表示由启动器按版本要求自动挑。
        // 特意不做 Java 自动下载 —— 自己挑厂商、自己管版本，本来就是玩 MC 的基本功。
        QString javaPath;
        bool pinned = false;
    };

    void load();
    void save() const;
    int indexOf(const QString &id) const;

    QList<Entry> m_entries;
};
