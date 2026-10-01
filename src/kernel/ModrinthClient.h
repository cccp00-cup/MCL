#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

class QNetworkAccessManager;
class QNetworkReply;

// Modrinth 的只读客户端：搜索项目、取项目的版本列表。
//
// 实测国内直连 api.modrinth.com 是通的，所以这里不做镜像；将来若要接镜像，
// 改 baseUrl() 一处即可。
//
// 两类市场共用它，靠 `kind` 区分：
//   · "mod"     → 模组市场
//   · "modpack" → 整合包市场
class ModrinthClient : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    // 从「实例设置」进来时会被绑到那个实例上：这时模组直接装进去，不用再问"装到哪"
    Q_PROPERTY(QString targetInstance READ targetInstance WRITE setTargetInstance
                   NOTIFY targetInstanceChanged)

public:
    explicit ModrinthClient(QObject *parent = nullptr);

    QVariantList results() const { return m_results; }
    bool loading() const { return m_loading; }
    QString targetInstance() const { return m_targetInstance; }
    void setTargetInstance(const QString &instanceId);

    static QString baseUrl();

    // kind: "mod" / "modpack"；gameVersion / loader 为空表示不限
    Q_INVOKABLE void search(const QString &kind, const QString &query,
                            const QString &gameVersion = QString(),
                            const QString &loader = QString());
    Q_INVOKABLE void clear();

Q_SIGNALS:
    void targetInstanceChanged();
    void resultsChanged();
    void loadingChanged();
    void failed(const QString &error);

private:
    void setLoading(bool loading);
    void setError(const QString &error);

    QNetworkAccessManager *m_net = nullptr;
    QVariantList m_results;
    QString m_targetInstance;
    bool m_loading = false;
    quint64 m_requestId = 0; // 只认最后一次请求的结果
};
