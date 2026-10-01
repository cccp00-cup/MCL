#include "ModrinthClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

QString ModrinthClient::baseUrl()
{
    // 官方 api.modrinth.com 在国内基本连不上（本机还遇到 TLS 拦截），
    // 走 MCIM 的 Modrinth 镜像。想切回官方改这一处即可。
    return QStringLiteral("https://mod.mcimirror.top/modrinth/v2");
}

ModrinthClient::ModrinthClient(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
}

void ModrinthClient::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    Q_EMIT loadingChanged();
}

void ModrinthClient::setError(const QString &error)
{
    setLoading(false);
    Q_EMIT failed(error);
}

void ModrinthClient::clear()
{
    ++m_requestId; // 让在途请求的结果作废
    m_results.clear();
    setLoading(false);
    Q_EMIT resultsChanged();
}

void ModrinthClient::setTargetInstance(const QString &instanceId)
{
    if (m_targetInstance == instanceId)
        return;
    m_targetInstance = instanceId;
    Q_EMIT targetInstanceChanged();
}

void ModrinthClient::search(const QString &kind, const QString &query, const QString &gameVersion,
                            const QString &loader)
{
    // facets 是 Modrinth 的过滤语法：[[ "project_type:mod" ], [ "versions:1.20.1" ]]
    QJsonArray facets;
    facets.append(QJsonArray{ QStringLiteral("project_type:%1").arg(kind) });
    if (!gameVersion.isEmpty())
        facets.append(QJsonArray{ QStringLiteral("versions:%1").arg(gameVersion) });
    // Modrinth 把加载器也归在 categories 里（fabric / forge / neoforge / quilt）
    if (!loader.isEmpty())
        facets.append(QJsonArray{ QStringLiteral("categories:%1").arg(loader) });

    QUrlQuery queryString;
    if (!query.trimmed().isEmpty()) {
        queryString.addQueryItem(QStringLiteral("query"), query.trimmed());
    } else {
        // 空查询时给个通配，否则 Modrinth 会返回空
        queryString.addQueryItem(QStringLiteral("query"), QStringLiteral("*"));
    }
    queryString.addQueryItem(QStringLiteral("limit"), QStringLiteral("24"));
    queryString.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
    queryString.addQueryItem(QStringLiteral("facets"),
                             QString::fromUtf8(QJsonDocument(facets).toJson(QJsonDocument::Compact)));

    QUrl url(baseUrl() + QStringLiteral("/search"));
    url.setQuery(queryString);

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    // Modrinth 要求带可识别的 User-Agent
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("mcl/0.1 (github.com/cccp00-cup/mcl)"));

    const quint64 id = ++m_requestId;
    setLoading(true);

    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, id]() {
        reply->deleteLater();
        if (id != m_requestId)
            return; // 已经有更新的检索了

        if (reply->error() != QNetworkReply::NoError) {
            setError(QStringLiteral("Modrinth 检索失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantList results;
        for (const QJsonValue &value : root.value(QStringLiteral("hits")).toArray()) {
            const QJsonObject hit = value.toObject();
            QVariantMap item;
            item[QStringLiteral("id")] = hit.value(QStringLiteral("project_id")).toString();
            item[QStringLiteral("slug")] = hit.value(QStringLiteral("slug")).toString();
            item[QStringLiteral("title")] = hit.value(QStringLiteral("title")).toString();
            item[QStringLiteral("description")] = hit.value(QStringLiteral("description")).toString();
            item[QStringLiteral("author")] = hit.value(QStringLiteral("author")).toString();
            item[QStringLiteral("downloads")] = hit.value(QStringLiteral("downloads")).toInt();
            item[QStringLiteral("follows")] = hit.value(QStringLiteral("follows")).toInt();
            item[QStringLiteral("icon")] = hit.value(QStringLiteral("icon_url")).toString();
            item[QStringLiteral("page")] = QStringLiteral("https://modrinth.com/%1/%2")
                                               .arg(hit.value(QStringLiteral("project_type")).toString(),
                                                    hit.value(QStringLiteral("slug")).toString());
            results << item;
        }

        m_results = results;
        setLoading(false);
        Q_EMIT resultsChanged();
    });
}
