#include "Net.h"
#include "AccountManager.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace
{
// 注意：全部是 login.live.com —— client_id 00000000402b5328 是 MSA 的客户端，
// 拿去请求 login.microsoftonline.com（Azure AD）会被 AADSTS700016 直接拒掉。
// 设备码端点。注意是 login.live.com 而不是 login.microsoftonline.com ——
// 后者是 Azure AD 的，00000000402b5328 在那边不存在（AADSTS700016）。
const char *kDeviceCodeUrl = "https://login.live.com/oauth20_connect.srf";
const char *kTokenUrl = "https://login.live.com/oauth20_token.srf";
const char *kXboxAuthUrl = "https://user.auth.xboxlive.com/user/authenticate";
const char *kXstsUrl = "https://xsts.auth.xboxlive.com/xsts/authorize";
const char *kMinecraftLoginUrl =
    "https://api.minecraftservices.com/authentication/login_with_xbox";
const char *kMinecraftProfileUrl = "https://api.minecraftservices.com/minecraft/profile";

// 离线账户的 UUID：Mojang 的约定算法，和 LaunchPlan 里那份保持一致
QString offlineUuid(const QString &playerName)
{
    const QByteArray raw =
        QCryptographicHash::hash(("OfflinePlayer:" + playerName).toUtf8(), QCryptographicHash::Md5)
            .toHex();
    QString hex = QString::fromLatin1(raw);
    if (hex.size() < 32)
        return QStringLiteral("00000000000000000000000000000000");
    hex[12] = QLatin1Char('3');
    const char ch = hex.at(16).toLatin1();
    hex[16] = QLatin1String("89ab")[int(ch) & 0x3];
    return hex.left(8) + QLatin1Char('-') + hex.mid(8, 4) + QLatin1Char('-') + hex.mid(12, 4)
        + QLatin1Char('-') + hex.mid(16, 4) + QLatin1Char('-') + hex.mid(20, 12);
}

QNetworkRequest jsonRequest(const QString &url)
{
    QNetworkRequest request{ QUrl(url) };
    Net::configure(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QByteArray jsonBody(const QJsonObject &object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
} // namespace

QString AccountManager::clientId()
{
    // Xbox Live 的公开客户端 id。OAuth 的公开客户端不靠 secret 保密（没有 secret 可泄），
    // 社区里的第三方启动器普遍在用这一个。
    return QStringLiteral("00000000402b5328");
}

QString AccountManager::storePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/account.json");
}

AccountManager::AccountManager(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
{
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        m_elapsedMs += m_pollTimer->interval();
        if (m_expiresInMs > 0 && m_elapsedMs > m_expiresInMs) {
            m_pollTimer->stop();
            setBusy(false);
            setStatus(QString());
            Q_EMIT failed(QStringLiteral("设备码超时了，重新来一次"));
            return;
        }
        // 按服务端给的 interval 去问，别问太勤
        if (m_elapsedMs % m_pollIntervalMs < m_pollTimer->interval())
            pollToken();
    });

    load();
}

AccountManager::~AccountManager() = default;

QString AccountManager::menuLabel() const
{
    if (m_kind == QLatin1String("offline"))
        return QStringLiteral("离线账户：%1").arg(m_name);
    if (m_kind == QLatin1String("microsoft"))
        return QStringLiteral("微软账户：%1").arg(m_name);
    return QStringLiteral("未登录");
}

QString AccountManager::accessToken() const
{
    // 离线账户的 token 就是约定的 "0"
    return m_accessToken.isEmpty() ? QStringLiteral("0") : m_accessToken;
}

QString AccountManager::userType() const
{
    return m_kind == QLatin1String("microsoft") ? QStringLiteral("msa") : QStringLiteral("legacy");
}

void AccountManager::setStatus(const QString &text)
{
    if (m_status == text)
        return;
    m_status = text;
    Q_EMIT changed();
}

void AccountManager::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    Q_EMIT changed();
}

void AccountManager::finish(const QString &kind, const QString &name, const QString &uuid,
                            const QString &accessToken)
{
    m_kind = kind;
    m_name = name;
    m_uuid = uuid;
    m_accessToken = accessToken;
    setBusy(false);
    setStatus(QString());
    save();
    Q_EMIT changed();
}

void AccountManager::signOut()
{
    cancel();
    m_kind.clear();
    m_name.clear();
    m_uuid.clear();
    m_accessToken.clear();
    m_refreshToken.clear();
    m_expiresAtMs = 0;
    save();
    Q_EMIT identityChanged();
}

void AccountManager::signInOffline(const QString &name)
{
    const QString player = name.trimmed().isEmpty() ? QStringLiteral("Player") : name.trimmed();
    if (player.size() > 16) {
        Q_EMIT failed(QStringLiteral("离线名字最多 16 个字符"));
        return;
    }
    cancel();
    finish(QStringLiteral("offline"), player, offlineUuid(player), QStringLiteral("0"));
}

void AccountManager::signInMicrosoft()
{
    cancel();
    setBusy(true);
    setStatus(QStringLiteral("正在申请设备码"));
    requestDeviceCode();
}

void AccountManager::cancel()
{
    m_pollTimer->stop();
    m_deviceCode.clear();
    m_elapsedMs = 0;
    setBusy(false);
    setStatus(QString());
}

void AccountManager::requestDeviceCode()
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("scope"), QStringLiteral("XboxLive.signin offline_access"));
    // 这个参数不能少 —— 少了 login.live.com 会按别的流程解释
    form.addQueryItem(QStringLiteral("response_type"), QStringLiteral("device_code"));

    QNetworkRequest request{ QUrl(QString::fromLatin1(kDeviceCodeUrl)) };
    Net::configure(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    QNetworkReply *reply = m_net->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();

        const QString userCode = root.value(QStringLiteral("user_code")).toString();
        const QString deviceCode = root.value(QStringLiteral("device_code")).toString();
        if (userCode.isEmpty() || deviceCode.isEmpty()) {
            setBusy(false);
            setStatus(QString());
            const QString description = root.value(QStringLiteral("error_description")).toString();
            Q_EMIT failed(description.isEmpty()
                              ? QStringLiteral("申请设备码失败 —— 检查一下网络")
                              : QStringLiteral("申请设备码失败：%1").arg(description));
            return;
        }

        m_deviceCode = deviceCode;
        m_verificationUri = root.value(QStringLiteral("verification_uri")).toString(
            QStringLiteral("https://www.microsoft.com/link"));
        m_pollIntervalMs = qMax(1, root.value(QStringLiteral("interval")).toInt(5)) * 1000;
        m_expiresInMs = root.value(QStringLiteral("expires_in")).toInt(900) * 1000;
        m_elapsedMs = 0;

        // 和大多数 Minecraft 启动器一样：码直接塞进剪贴板，浏览器自己弹出来。
        // 用户切过去按一下粘贴就行，不用在两块屏之间手抄 8 位码。
        m_lastUserCode = userCode;
        copyDeviceCode(userCode);
        const bool opened = QDesktopServices::openUrl(QUrl(m_verificationUri));

        setStatus(opened
                      ? QStringLiteral("已在浏览器打开登录页，设备码已复制到剪贴板")
                      : QStringLiteral("设备码已复制，请手动打开 %1").arg(m_verificationUri));

        Q_EMIT deviceCodeReady(userCode, m_verificationUri);
        m_pollTimer->start();
    });
}

// 把设备码放进剪贴板。非 GUI 环境（比如 offscreen 跑冒烟测试）下剪贴板不存在，
// 这里全部做判空，绝不能因为复制个码把登录流程搞崩。
void AccountManager::copyDeviceCode(const QString &userCode)
{
    if (userCode.isEmpty())
        return;
    if (!qobject_cast<QGuiApplication *>(QCoreApplication::instance()))
        return;
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard)
        return;

    clipboard->setText(userCode, QClipboard::Clipboard);
    // Linux 下顺手把主选区也写上：中键一贴就有，不用先 Ctrl+V
    if (clipboard->supportsSelection())
        clipboard->setText(userCode, QClipboard::Selection);
}

void AccountManager::copyLastDeviceCode()
{
    copyDeviceCode(m_lastUserCode);
}

// 用户在界面里点"重新打开浏览器"时用
void AccountManager::reopenVerificationPage()
{
    if (m_verificationUri.isEmpty())
        return;
    copyDeviceCode(m_lastUserCode);
    QDesktopServices::openUrl(QUrl(m_verificationUri));
}

void AccountManager::pollToken()
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("device_code"), m_deviceCode);
    form.addQueryItem(QStringLiteral("grant_type"),
                      QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));

    QNetworkRequest request{ QUrl(QString::fromLatin1(kTokenUrl)) };
    Net::configure(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    QNetworkReply *reply = m_net->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();

        // 还没点完就是 authorization_pending，属于正常状态，下一拍继续问
        const QString error = root.value(QStringLiteral("error")).toString();
        if (error == QLatin1String("authorization_pending"))
            return;
        if (error == QLatin1String("slow_down")) {
            m_pollIntervalMs += 2000;   // 服务端嫌我们问得太勤
            return;
        }
        if (!error.isEmpty()) {
            m_pollTimer->stop();
            setBusy(false);
            setStatus(QString());
            QString message;
            if (error == QLatin1String("expired_token"))
                message = QStringLiteral("设备码过期了，重新来一次");
            else if (error == QLatin1String("authorization_declined"))
                message = QStringLiteral("登录被拒绝了");
            else
                message = QStringLiteral("登录失败：%1").arg(
                    root.value(QStringLiteral("error_description")).toString(error));
            Q_EMIT failed(message);
            return;
        }

        const QString msa = root.value(QStringLiteral("access_token")).toString();
        if (msa.isEmpty())
            return;   // 还没好，继续轮询

        m_pollTimer->stop();
        m_refreshToken = root.value(QStringLiteral("refresh_token")).toString();
        const int expiresIn = root.value(QStringLiteral("expires_in")).toInt(3600);
        m_expiresAtMs = QDateTime::currentMSecsSinceEpoch() + qint64(expiresIn) * 1000;

        setStatus(QStringLiteral("正在认证 Xbox Live"));
        authenticateXbox(msa);
    });
}

// 用 refresh_token 续一条新的 MSA 令牌，走完剩下的认证链
void AccountManager::refreshAccessToken()
{
    if (m_refreshToken.isEmpty()) {
        setBusy(false);
        Q_EMIT tokenExpired();
        return;
    }

    setStatus(QStringLiteral("正在续期登录令牌"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), clientId());
    form.addQueryItem(QStringLiteral("refresh_token"), m_refreshToken);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    QNetworkRequest request{ QUrl(QString::fromLatin1(kTokenUrl)) };
    Net::configure(request);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    QNetworkReply *reply = m_net->post(request, form.toString(QUrl::FullyEncoded).toUtf8());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString msa = root.value(QStringLiteral("access_token")).toString();
        if (msa.isEmpty()) {
            setBusy(false);
            setStatus(QString());
            Q_EMIT tokenExpired();
            return;
        }
        m_refreshToken = root.value(QStringLiteral("refresh_token")).toString(m_refreshToken);
        const int expiresIn = root.value(QStringLiteral("expires_in")).toInt(3600);
        m_expiresAtMs = QDateTime::currentMSecsSinceEpoch() + qint64(expiresIn) * 1000;
        authenticateXbox(msa);
    });
}

void AccountManager::authenticateXbox(const QString &msaToken)
{
    QJsonObject properties;
    properties[QStringLiteral("AuthMethod")] = QStringLiteral("RPS");
    properties[QStringLiteral("SiteName")] = QStringLiteral("user.auth.xboxlive.com");
    properties[QStringLiteral("RpsTicket")] = QStringLiteral("d=") + msaToken;

    QJsonObject body;
    body[QStringLiteral("Properties")] = properties;
    body[QStringLiteral("RelyingParty")] = QStringLiteral("http://auth.xboxlive.com");
    body[QStringLiteral("TokenType")] = QStringLiteral("JWT");

    QNetworkReply *reply = m_net->post(jsonRequest(QString::fromLatin1(kXboxAuthUrl)),
                                       jsonBody(body));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false);
            Q_EMIT failed(QStringLiteral("Xbox Live 认证失败：%1").arg(reply->errorString()));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token = root.value(QStringLiteral("Token")).toString();
        if (token.isEmpty()) {
            setBusy(false);
            Q_EMIT failed(QStringLiteral("Xbox Live 没返回令牌"));
            return;
        }
        setStatus(QStringLiteral("正在向 XSTS 换取授权"));
        authorizeXsts(token);
    });
}

void AccountManager::authorizeXsts(const QString &xblToken)
{
    QJsonObject properties;
    properties[QStringLiteral("SandboxId")] = QStringLiteral("RETAIL");
    properties[QStringLiteral("UserTokens")] = QJsonArray{ xblToken };

    QJsonObject body;
    body[QStringLiteral("Properties")] = properties;
    body[QStringLiteral("RelyingParty")] = QStringLiteral("rp://api.minecraftservices.com/");
    body[QStringLiteral("TokenType")] = QStringLiteral("JWT");

    QNetworkReply *reply =
        m_net->post(jsonRequest(QString::fromLatin1(kXstsUrl)), jsonBody(body));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray payload = reply->readAll();

        if (reply->error() != QNetworkReply::NoError) {
            // XSTS 的报错很有代表性，值得翻成人话
            const QJsonObject root = QJsonDocument::fromJson(payload).object();
            const int xerr = root.value(QStringLiteral("XErr")).toInt(0);
            QString hint = QStringLiteral("XSTS 授权失败：%1").arg(reply->errorString());
            if (xerr == 2148916233)
                hint = QStringLiteral("这个微软账户还没开通 Xbox 档案，先去 xbox.com 建一个");
            else if (xerr == 2148916235)
                hint = QStringLiteral("这个账户所在地区不支持 Xbox Live");
            else if (xerr == 2148916238)
                hint = QStringLiteral("未成年账户需要先加入家庭组");
            setBusy(false);
            Q_EMIT failed(hint);
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(payload).object();
        const QString token = root.value(QStringLiteral("Token")).toString();
        const QJsonArray xui = root.value(QStringLiteral("DisplayClaims"))
                                   .toObject()
                                   .value(QStringLiteral("xui"))
                                   .toArray();
        const QString uhs =
            xui.isEmpty() ? QString() : xui.first().toObject().value(QStringLiteral("uhs")).toString();

        if (token.isEmpty() || uhs.isEmpty()) {
            setBusy(false);
            Q_EMIT failed(QStringLiteral("XSTS 没返回完整的授权信息"));
            return;
        }
        setStatus(QStringLiteral("正在登录 Minecraft 服务"));
        loginMinecraft(token, uhs);
    });
}

void AccountManager::loginMinecraft(const QString &xstsToken, const QString &uhs)
{
    QJsonObject body;
    // 这个格式是 Minecraft 服务要求的，不能改
    body[QStringLiteral("identityToken")] =
        QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, xstsToken);

    QNetworkReply *reply =
        m_net->post(jsonRequest(QString::fromLatin1(kMinecraftLoginUrl)), jsonBody(body));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false);
            Q_EMIT failed(QStringLiteral("登录 Minecraft 失败：%1").arg(reply->errorString()));
            return;
        }
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString token = root.value(QStringLiteral("access_token")).toString();
        if (token.isEmpty()) {
            setBusy(false);
            Q_EMIT failed(QStringLiteral("Minecraft 服务没返回 access_token"));
            return;
        }
        m_accessToken = token;
        setStatus(QStringLiteral("正在读取玩家资料"));
        fetchProfile();
    });
}

void AccountManager::fetchProfile()
{
    QNetworkRequest request{ QUrl(QString::fromLatin1(kMinecraftProfileUrl)) };
    Net::configure(request);
    request.setRawHeader("Authorization", "Bearer " + m_accessToken.toUtf8());
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            setBusy(false);
            // 404 一般意味着这个账户没买过游戏
            const int status =
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            Q_EMIT failed(status == 404
                              ? QStringLiteral("这个账户没有 Minecraft: Java 版的授权")
                              : QStringLiteral("读玩家资料失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        const QString id = root.value(QStringLiteral("id")).toString();
        const QString name = root.value(QStringLiteral("name")).toString();
        if (id.isEmpty() || name.isEmpty()) {
            setBusy(false);
            Q_EMIT failed(QStringLiteral("玩家资料不完整"));
            return;
        }
        finish(QStringLiteral("microsoft"), name, id, m_accessToken);
    });
}

void AccountManager::save() const
{
    QJsonObject root;
    root[QStringLiteral("kind")] = m_kind;
    root[QStringLiteral("name")] = m_name;
    root[QStringLiteral("uuid")] = m_uuid;
    root[QStringLiteral("accessToken")] = m_accessToken;
    root[QStringLiteral("refreshToken")] = m_refreshToken;

    QDir().mkpath(QFileInfo(storePath()).absolutePath());
    QFile file(storePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void AccountManager::load()
{
    QFile file(storePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    m_kind = root.value(QStringLiteral("kind")).toString();
    m_name = root.value(QStringLiteral("name")).toString();
    m_uuid = root.value(QStringLiteral("uuid")).toString();
    m_accessToken = root.value(QStringLiteral("accessToken")).toString();
    m_refreshToken = root.value(QStringLiteral("refreshToken")).toString();

    // 离线账户的 uuid 是算出来的，存档里可能是空的（比如手工写进去的配置）
    if (m_kind == QLatin1String("offline") && m_uuid.isEmpty() && !m_name.isEmpty())
        m_uuid = offlineUuid(m_name);
}
