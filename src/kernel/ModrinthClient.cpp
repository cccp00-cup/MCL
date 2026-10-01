#include "Net.h"
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

void ModrinthClient::fetchVersions(const QString &projectId)
{
    if (projectId.isEmpty())
        return;

    qInfo("[mcl] 取版本列表 project=%s", qPrintable(projectId));

    m_projectVersions.clear();
    Q_EMIT projectVersionsChanged();
    m_versionsLoading = true;
    Q_EMIT versionsLoadingChanged();

    QNetworkRequest request{
        QUrl(baseUrl() + QStringLiteral("/project/%1/version").arg(projectId))
    };
    Net::configure(request);

    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, projectId]() {
        reply->deleteLater();
        m_versionsLoading = false;
        Q_EMIT versionsLoadingChanged();

        if (reply->error() != QNetworkReply::NoError) {
            setError(QStringLiteral("取版本列表失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonArray array = QJsonDocument::fromJson(reply->readAll()).array();
        QVariantList out;
        out.reserve(array.size());

        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();

            // 只留主文件；没有 primary 标记就退回第一个
            const QJsonArray files = object.value(QStringLiteral("files")).toArray();
            QString url;
            QString filename;
            qint64 size = 0;
            for (const QJsonValue &fileValue : files) {
                const QJsonObject file = fileValue.toObject();
                const bool primary = file.value(QStringLiteral("primary")).toBool();
                if (url.isEmpty() || primary) {
                    url = file.value(QStringLiteral("url")).toString();
                    filename = file.value(QStringLiteral("filename")).toString();
                    size = file.value(QStringLiteral("size")).toVariant().toLongLong();
                }
                if (primary)
                    break;
            }
            if (url.isEmpty())
                continue;   // 没有可下文件的版本对安装没意义

            QStringList gameVersions;
            for (const QJsonValue &v : object.value(QStringLiteral("game_versions")).toArray())
                gameVersions << v.toString();
            QStringList loaders;
            for (const QJsonValue &v : object.value(QStringLiteral("loaders")).toArray())
                loaders << v.toString();

            QVariantMap item;
            item[QStringLiteral("id")] = object.value(QStringLiteral("id")).toString();
            item[QStringLiteral("name")] = object.value(QStringLiteral("name")).toString();
            item[QStringLiteral("versionNumber")] =
                object.value(QStringLiteral("version_number")).toString();
            item[QStringLiteral("datePublished")] =
                object.value(QStringLiteral("date_published")).toString().left(10);
            item[QStringLiteral("downloads")] =
                object.value(QStringLiteral("downloads")).toVariant().toLongLong();
            item[QStringLiteral("gameVersions")] = gameVersions;
            item[QStringLiteral("loaders")] = loaders;
            item[QStringLiteral("gameVersionsText")] = gameVersions.join(QStringLiteral(", "));
            item[QStringLiteral("loadersText")] = loaders.join(QStringLiteral(", "));
            item[QStringLiteral("downloadUrl")] = url;
            item[QStringLiteral("filename")] = filename;
            item[QStringLiteral("size")] = size;
            out << item;
        }

        qInfo("[mcl] project=%s 拿到 %d 个可下载版本", qPrintable(projectId), int(out.size()));
        m_projectVersions = out;
        Q_EMIT projectVersionsChanged();
    });
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
    // facets 是 Modrinth 的过滤语法：[[ "project_type:mod" ], [ "game_versions:1.20.1" ]]
    //
    // 版本这一项**必须**叫 game_versions —— `versions` 是旧版 API 的名字，
    // 现在传过去会被静默忽略，结果是列表里混进一堆不支持当前版本的模组，
    // 装了也起不来。（实测 game_versions:0.0.0 返回 0 条，确认过滤真的生效。）
    QJsonArray facets;
    facets.append(QJsonArray{ QStringLiteral("project_type:%1").arg(kind) });
    if (!gameVersion.isEmpty())
        facets.append(QJsonArray{ QStringLiteral("game_versions:%1").arg(gameVersion) });
    // 加载器用 loaders —— 比 categories 准确（后者是个大杂烩，还混着玩法标签）
    if (!loader.isEmpty())
        facets.append(QJsonArray{ QStringLiteral("loaders:%1").arg(loader) });

    QUrlQuery queryString;
    if (!query.trimmed().isEmpty()) {
        queryString.addQueryItem(QStringLiteral("query"), query.trimmed());
    } else {
        // 空查询时给个通配，否则 Modrinth 会返回空
        queryString.addQueryItem(QStringLiteral("query"), QStringLiteral("*"));
    }
    queryString.addQueryItem(QStringLiteral("limit"), QStringLiteral("24"));
    queryString.addQueryItem(QStringLiteral("index"), QStringLiteral("downloads"));
    const QByteArray facetsJson = QJsonDocument(facets).toJson(QJsonDocument::Compact);
    queryString.addQueryItem(QStringLiteral("facets"), QString::fromUtf8(facetsJson));

    // 把实际发出去的过滤条件记一笔。筛选一旦静默失效，用户只会看到
    // "列表里怎么全是不支持我这个版本的模组"，而日志能直接定位到原因。
    qInfo("[mcl] 市场搜索 kind=%s 版本=%s 加载器=%s facets=%s",
          qPrintable(kind),
          qPrintable(gameVersion.isEmpty() ? QStringLiteral("不限") : gameVersion),
          qPrintable(loader.isEmpty() ? QStringLiteral("不限") : loader),
          facetsJson.constData());

    QUrl url(baseUrl() + QStringLiteral("/search"));
    url.setQuery(queryString);

    QNetworkRequest request(url);
    Net::configure(request);
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
