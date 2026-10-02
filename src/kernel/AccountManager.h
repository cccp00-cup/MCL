#pragma once

#include <QNetworkReply>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QTimer;

// 账户：离线 or 微软。
//
// 微软走**设备码流程**（device authorization grant）。
//
// 这里踩了两个坑，都值得记下来：
//
// 1. `00000000402b5328` 是 **MSA（login.live.com）** 的客户端，不是 Azure AD 的。
//    拿它去请求 `login.microsoftonline.com/consumers/oauth2/v2.0/devicecode` 会被直接拒：
//      AADSTS700016: Application with identifier '00000000402b5328' was not found
//    正确端点是 **`login.live.com/oauth20_connect.srf`**，还要带上
//    `response_type=device_code`。用错端点就是"申请设备码失败"。
//
// 2. 它也不接受 `http://localhost` 回调 —— 那个 redirect 只注册了
//    `https://login.live.com/oauth20_desktop.srf`，传 localhost 会拿到一个错误页。
//    所以授权码 + 本地回环那条路对它是走不通的（PrismLauncher 能走，是因为它注册了
//    自己的 Azure AD 应用）。设备码流程压根没有 redirect_uri，正好绕开这个限制。
//
// 完整链路：
//   oauth20_connect.srf 申请设备码 → 用户去 www.microsoft.com/link 输码
//     → 轮询 oauth20_token.srf → Xbox Live 认证 → XSTS 授权
//     → Minecraft 登录 → 拉 profile（拿 uuid 和玩家名）
class AccountManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString kind READ kind NOTIFY changed)           // "offline" / "microsoft" / ""
    Q_PROPERTY(QString playerName READ playerName NOTIFY changed)
    Q_PROPERTY(QString uuid READ uuid NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)       // 登录过程中的提示
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool signedIn READ signedIn NOTIFY changed)
    // 苹果菜单里显示的那一行
    Q_PROPERTY(QString menuLabel READ menuLabel NOTIFY changed)
    // 当前这轮的设备码。内置浏览器会把它顶在页面上方，省得用户切回启动器找。
    // 登录完成或登出后清空。
    Q_PROPERTY(QString deviceCode READ deviceCode NOTIFY changed)

public:
    explicit AccountManager(QObject *parent = nullptr);
    ~AccountManager() override;

    static QString storePath();
    static QString clientId();

    QString kind() const { return m_kind; }
    QString playerName() const { return m_name; }
    QString uuid() const { return m_uuid; }
    QString status() const { return m_status; }
    bool busy() const { return m_busy; }
    bool signedIn() const { return !m_kind.isEmpty(); }
    QString menuLabel() const;
    QString deviceCode() const { return m_lastUserCode; }

    // 启动游戏时用的凭据
    QString accessToken() const;
    QString userType() const;

    Q_INVOKABLE void signInOffline(const QString &name);
    Q_INVOKABLE void signInMicrosoft();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void signOut();
    // 重新打开登录页（顺便再复制一次设备码）
    Q_INVOKABLE void reopenVerificationPage();
    // 只把设备码再复制一次（万一剪贴板被别的东西顶掉了）
    Q_INVOKABLE void copyLastDeviceCode();

Q_SIGNALS:
    void changed();
    // 身份变了（登录/登出/换账户），界面该刷新菜单标题
    void identityChanged();
    void failed(const QString &error);
    // 设备码就绪，界面该把验证页打开给用户。
    // **这里不自己开浏览器** —— 交给界面决定：有内置浏览器就用它，
    // 没有才回退到系统浏览器。（重新打开验证页也走同一个信号。）
    void deviceCodeReady(const QString &userCode, const QString &verificationUri);
    // 令牌已经过期且续不回来，得重新登录
    void tokenExpired();

private:
    void load();
    void save() const;
    void setStatus(const QString &text);
    void setBusy(bool busy);
    void finish(const QString &kind, const QString &name, const QString &uuid,
                const QString &accessToken);

    void requestDeviceCode();
    void pollToken();
    void copyDeviceCode(const QString &userCode);
    void refreshAccessToken();
    void authenticateXbox(const QString &msaToken);
    void authorizeXsts(const QString &xblToken);
    void loginMinecraft(const QString &xstsToken, const QString &uhs);
    void fetchProfile();

    QNetworkAccessManager *m_net = nullptr;

    QString m_kind;
    QString m_name;
    QString m_uuid;
    QString m_accessToken;
    QString m_refreshToken;

    QString m_status;
    bool m_busy = false;

    // 设备码流程的中间状态
    QString m_deviceCode;
    QString m_verificationUri;
    QString m_lastUserCode;
    int m_pollIntervalMs = 5000;
    int m_expiresInMs = 0;
    int m_elapsedMs = 0;
    // access token 什么时候过期（到期前用 refresh_token 续）
    qint64 m_expiresAtMs = 0;
    QTimer *m_pollTimer = nullptr;
};
