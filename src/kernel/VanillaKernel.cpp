#include "VanillaKernel.h"

#include "Downloader.h"
#include "AccountManager.h"
#include "InstanceStore.h"
#include "Mirror.h"
#include "LaunchPlan.h"
#include "ZipExtract.h"

#include <QDir>
#include <QElapsedTimer>
#include <QDesktopServices>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>

namespace
{
const char *kManifestUrl = "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json";
}

VanillaKernel::VanillaKernel(InstanceStore *store, AccountManager *account, QObject *parent)
    : McKernel(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_downloader(new Downloader(this))
    , m_store(store)
    , m_account(account)
{
    if (m_store)
        connect(m_store, &InstanceStore::instancesChanged, this, &VanillaKernel::refresh);

    connect(m_downloader, &Downloader::progress, this,
            [this](int done, int total, qint64 bytes, qint64 totalBytes) {
                Q_EMIT downloadProgress(done, total, bytes, totalBytes);
                // 阶段文字刻意不带字节数：带了会让 setStage 每次都判定为"变了"，
                // 日志被进度刷屏。字节数走 downloadProgress，界面自己显示。
                if (total > 1)
                    setStage(QStringLiteral("下载中 %1/%2").arg(done).arg(total));
            });
    connect(m_downloader, &Downloader::finished, this, &VanillaKernel::onDownloadFinished);
    connect(m_downloader, &Downloader::note, this,
            [this](const QString &text) { Q_EMIT logLine(QStringLiteral("[mcl] ") + text); });

    QDir().mkpath(dataDir());
    detectJava();

    // 本地已有清单就直接读，没有就后台拉一份 —— 界面一开始就要能列出可安装的版本
    if (QFileInfo::exists(manifestPath())) {
        QFile file(manifestPath());
        if (file.open(QIODevice::ReadOnly)) {
            m_manifest = McVersion::parseManifest(QJsonDocument::fromJson(file.readAll()).object());
            m_manifestLoaded = !m_manifest.isEmpty();
            m_versionsCache.clear();
        }
    }
    refresh();

    // 清单没在本地就后台拉。拉的过程里「新建实例」窗口是能开的，
    // 只是列表空着并显示一个转圈 —— 千万别在这儿同步等网络。
    if (!m_manifestLoaded)
        prefetchManifest();
}

// 后台把清单拉下来。刻意**不走 Downloader** —— 那个是启动流程专用的，
// 走它会和用户的启动请求抢通道（曾经因此让点启动直接失败）。
void VanillaKernel::prefetchManifest()
{
    QNetworkRequest request{ QUrl(Mirror::rewrite(QString::fromLatin1(kManifestUrl))) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError)
            return;

        const QByteArray payload = reply->readAll();
        QFile file(manifestPath());
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            file.write(payload);
            file.close();
        }

        m_manifest = McVersion::parseManifest(QJsonDocument::fromJson(payload).object());
        m_manifestLoaded = !m_manifest.isEmpty();
        m_versionsCache.clear();
        if (m_manifestLoaded) {
            Q_EMIT manifestChanged();
            refresh();
            // 如果预取期间用户已经点了启动，这时候接着往下走
            // （交给 resumeInstall：它知道该补的是自己还是某个父版本）
            if (!m_activeVersionId.isEmpty() && !m_downloader->isRunning())
                resumeInstall();
        }
    });
}

VanillaKernel::~VanillaKernel()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->terminate();
        if (!m_process->waitForFinished(3000))
            m_process->kill();
    }
}

// ——————————————————————————————————————————————— 路径

QString VanillaKernel::dataDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString VanillaKernel::mergedDir() const
{
    // 每个实例有自己的游戏目录（saves/mods/config 都在里面）；
    // 直接装某个版本（没有实例）时退回共享目录。
    return m_activeGameDir.isEmpty() ? dataDir() : m_activeGameDir;
}

QString VanillaKernel::versionsDir() const
{
    return dataDir() + QStringLiteral("/versions");
}

QString VanillaKernel::librariesDir() const
{
    return dataDir() + QStringLiteral("/libraries");
}

QString VanillaKernel::assetsDir() const
{
    return dataDir() + QStringLiteral("/assets");
}

QString VanillaKernel::nativesDirFor(const QString &versionId) const
{
    return dataDir() + QStringLiteral("/natives/") + versionId;
}

QString VanillaKernel::versionJsonPath(const QString &versionId) const
{
    return versionsDir() + QLatin1Char('/') + versionId + QLatin1Char('/') + versionId
        + QStringLiteral(".json");
}

QString VanillaKernel::clientJarPath(const QString &versionId) const
{
    return versionsDir() + QLatin1Char('/') + versionId + QLatin1Char('/') + versionId
        + QStringLiteral(".jar");
}

QString VanillaKernel::manifestPath() const
{
    return dataDir() + QStringLiteral("/version_manifest_v2.json");
}

QString VanillaKernel::dataPath() const
{
    return dataDir();
}

// ——————————————————————————————————————————————— 基本信息

QString VanillaKernel::name() const
{
    return QStringLiteral("自研 Vanilla 内核");
}

QString VanillaKernel::unavailableReason() const
{
    return QString();
}

QString VanillaKernel::javaInfo() const
{
    if (m_java.isEmpty())
        return QStringLiteral("未找到 Java");

    QStringList parts;
    for (int i = 0; i < m_java.size() && i < 4; ++i)
        parts << m_java.at(i).label();
    return parts.join(QStringLiteral(" · "));
}

void VanillaKernel::detectJava()
{
    m_java = JavaLocator::findAll();
    Q_EMIT javaInfoChanged();
}

bool VanillaKernel::isRunning(const QString &instanceId) const
{
    return !m_runningInstanceId.isEmpty() && m_runningInstanceId == instanceId && m_process
        && m_process->state() != QProcess::NotRunning;
}

void VanillaKernel::setStage(const QString &text)
{
    if (m_stage == text)
        return;
    m_stage = text;
    Q_EMIT stageChanged(text);
    Q_EMIT logLine(QStringLiteral("[mcl] %1").arg(text));
}

void VanillaKernel::fail(const QString &error)
{
    m_step = Step::None;
    setStage(QStringLiteral("失败：%1").arg(error));
    Q_EMIT launchFailed(m_activeInstanceId, error);
}

// ——————————————————————————————————————————————— 实例列表

void VanillaKernel::refresh()
{
    QVariantList list;

    if (m_store) {
        const QVariantList instances = m_store->instances();
        for (const QVariant &value : instances) {
            QVariantMap item = value.toMap();
            const QString instanceId = item.value(QStringLiteral("id")).toString();
            const QString versionId = item.value(QStringLiteral("versionId")).toString();
            const bool installed = QFileInfo::exists(versionJsonPath(versionId));
            const bool running = isRunning(instanceId);

            item[QStringLiteral("running")] = running;
            item[QStringLiteral("installed")] = installed;
            item[QStringLiteral("state")] = running      ? QStringLiteral("运行中")
                : installed                              ? QStringLiteral("已就绪")
                                                         : QStringLiteral("待下载");
            list << item;
        }
    }

    setInstances(list);
    setAvailability(true, QString());
}

// 返回清单里的**全部**版本（含快照和远古版本），筛选和搜索交给界面做。
// 之前这里只放正式版、还砍到 12 条，结果"新建实例"根本看不到完整列表。
// 返回清单里的**全部**版本（含快照和远古版本），筛选和搜索交给界面做。
// 之前这里只放正式版、还砍到 12 条，结果"新建实例"根本看不到完整列表。
//
// 结果会缓存：QML 里 var 属性的绑定会被反复求值，而重建 900+ 个 QVariantMap
// 一次就要 9ms 左右，累起来能吃掉十几秒。清单变了才需要重算。
QVariantList VanillaKernel::availableVersions() const
{
    if (!m_versionsCache.isEmpty() || m_manifest.isEmpty())
        return m_versionsCache;

    m_versionsCache.reserve(m_manifest.size());
    for (const McManifestEntry &entry : m_manifest) {
        QVariantMap item;
        item[QStringLiteral("id")] = entry.id;
        item[QStringLiteral("name")] = entry.id;
        item[QStringLiteral("type")] = entry.type;
        item[QStringLiteral("released")] = entry.releaseTime.left(10);
        m_versionsCache << item;
    }
    return m_versionsCache;
}

// ——————————————————————————————————————————————— 实例增删改
// 界面只认 kernel 一个入口，具体落盘交给 InstanceStore

QString VanillaKernel::createInstance(const QString &versionId, const QString &name)
{
    if (!m_store)
        return {};
    const QString id = m_store->create(name, versionId, QStringLiteral("Vanilla"));
    refresh();
    return id;
}


// ——————————————————————————————————————————————— 整合包（Modrinth .mrpack）

namespace
{
QString modrinthBase()
{
    return QStringLiteral("https://mod.mcimirror.top/modrinth/v2");
}

// Modrinth 的 CDN（cdn.modrinth.com）在国内证书对不上，本机还会被 TLS 拦，
// 走 MCIM 的文件镜像 —— 路径一模一样，只换域名。
QString mirrorModrinthFile(const QString &url)
{
    static const QString cdn = QStringLiteral("https://cdn.modrinth.com");
    if (url.startsWith(cdn))
        return QStringLiteral("https://mod.mcimirror.top") + url.mid(cdn.size());
    return url;
}

QNetworkRequest modrinthRequest(const QString &url)
{
    QNetworkRequest request{ QUrl(url) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("mcl/0.1 (github.com/cccp00-cup/mcl)"));
    return request;
}
} // namespace

void VanillaKernel::installModpack(const QString &projectId, const QString &title)
{
    if (m_pack.active) {
        fail(QStringLiteral("已经在装一个整合包了，等它结束"));
        return;
    }
    m_pack = PendingPack{};
    m_pack.projectId = projectId;
    m_pack.title = title;
    m_pack.active = true;
    fetchPackVersion(projectId, title);
}

void VanillaKernel::fetchPackVersion(const QString &projectId, const QString &title, int attempt)
{
    setStage(QStringLiteral("查询整合包 %1 的版本").arg(title.isEmpty() ? projectId : title));

    // 顺便把项目信息拿下来 —— 整合包的图标是它在 Modrinth 上设的那个
    if (m_pack.iconUrl.isEmpty()) {
        QNetworkReply *info = m_net->get(
            modrinthRequest(modrinthBase() + QStringLiteral("/project/%1").arg(projectId)));
        connect(info, &QNetworkReply::finished, this, [this, info]() {
            info->deleteLater();
            if (info->error() != QNetworkReply::NoError)
                return; // 拿不到就算了，回头用默认图标
            const QJsonObject project = QJsonDocument::fromJson(info->readAll()).object();
            m_pack.iconUrl = project.value(QStringLiteral("icon_url")).toString();
            m_pack.title = project.value(QStringLiteral("title")).toString(m_pack.title);
        });
    }

    QNetworkReply *reply =
        m_net->get(modrinthRequest(modrinthBase() + QStringLiteral("/project/%1/version").arg(projectId)));
    connect(reply, &QNetworkReply::finished, this, [this, reply, projectId, title, attempt]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            // 镜像会偶发抽风（实测同一个地址三次里会挂一次），重试几次再说
            if (attempt < 3) {
                Q_EMIT logLine(QStringLiteral("[mcl] 查版本失败（%1），重试第 %2 次")
                                   .arg(reply->errorString())
                                   .arg(attempt));
                fetchPackVersion(projectId, title, attempt + 1);
                return;
            }
            fail(QStringLiteral("查整合包版本失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonArray versions = QJsonDocument::fromJson(reply->readAll()).array();
        QString packUrl;
        QString versionNumber;
        for (const QJsonValue &value : versions) {
            const QJsonObject version = value.toObject();
            for (const QJsonValue &fv : version.value(QStringLiteral("files")).toArray()) {
                const QJsonObject file = fv.toObject();
                // 一个版本里可能同时挂 .mrpack 和其它附件
                if (!file.value(QStringLiteral("filename")).toString().endsWith(
                        QLatin1String(".mrpack")))
                    continue;
                packUrl = file.value(QStringLiteral("url")).toString();
                versionNumber = version.value(QStringLiteral("version_number")).toString();
                break;
            }
            if (!packUrl.isEmpty())
                break;
        }

        if (packUrl.isEmpty()) {
            fail(QStringLiteral("这个整合包没有可下载的 .mrpack"));
            return;
        }

        m_pack.title = title.isEmpty() ? QStringLiteral("整合包") : title;
        m_pack.slug = projectId;
        m_pack.archivePath = dataDir() + QStringLiteral("/modpacks/") + projectId
            + QStringLiteral(".mrpack");

        setStage(QStringLiteral("下载整合包 %1（%2）").arg(m_pack.title, versionNumber));
        m_step = Step::PackDownload;
        DownloadItem item;
        item.url = mirrorModrinthFile(packUrl);
        item.fallbackUrl = packUrl;
        item.targetPath = m_pack.archivePath;
        item.label = QStringLiteral("%1.mrpack").arg(m_pack.title);
        m_downloader->start({ item }, 1);
    });
}

void VanillaKernel::downloadPack()
{
    // Step::PackDownload 完成后落到这里：解压 + 解析 index
    unpackAndPlanPack();
}

void VanillaKernel::unpackAndPlanPack()
{
    m_pack.extractDir = dataDir() + QStringLiteral("/modpacks/") + m_pack.projectId
        + QStringLiteral("_unpacked");
    QDir(m_pack.extractDir).removeRecursively();
    QDir().mkpath(m_pack.extractDir);

    setStage(QStringLiteral("解压整合包"));
    QString error;
    if (!ZipExtract::extract(m_pack.archivePath, m_pack.extractDir, {}, &error)) {
        fail(QStringLiteral("解压整合包失败：%1").arg(error));
        return;
    }

    QFile index(m_pack.extractDir + QStringLiteral("/modrinth.index.json"));
    if (!index.open(QIODevice::ReadOnly)) {
        fail(QStringLiteral("整合包里没有 modrinth.index.json"));
        return;
    }

    const QJsonObject root = QJsonDocument::fromJson(index.readAll()).object();
    m_pack.title = root.value(QStringLiteral("name")).toString(m_pack.title);

    const QJsonObject deps = root.value(QStringLiteral("dependencies")).toObject();
    m_pack.minecraft = deps.value(QStringLiteral("minecraft")).toString();
    if (deps.contains(QStringLiteral("fabric-loader"))) {
        m_pack.loaderKind = QStringLiteral("fabric");
        m_pack.loaderVersion = deps.value(QStringLiteral("fabric-loader")).toString();
    } else if (deps.contains(QStringLiteral("neoforge"))) {
        m_pack.loaderKind = QStringLiteral("neoforge");
        m_pack.loaderVersion = deps.value(QStringLiteral("neoforge")).toString();
    } else if (deps.contains(QStringLiteral("forge"))) {
        m_pack.loaderKind = QStringLiteral("forge");
        m_pack.loaderVersion = deps.value(QStringLiteral("forge")).toString();
    }

    if (m_pack.minecraft.isEmpty()) {
        fail(QStringLiteral("整合包没声明要哪个 Minecraft 版本"));
        return;
    }

    // index.json 里列的文件：每条给 path（相对实例目录）+ 若干下载地址
    m_pack.files.clear();
    for (const QJsonValue &fv : root.value(QStringLiteral("files")).toArray()) {
        const QJsonObject file = fv.toObject();
        const QString path = file.value(QStringLiteral("path")).toString();
        const QJsonArray urls = file.value(QStringLiteral("downloads")).toArray();
        if (path.isEmpty() || urls.isEmpty())
            continue;
        DownloadItem item;
        // 优先用镜像地址；官方的留在 fallbackUrl 上，镜像挂了能自动换回去
        item.url = mirrorModrinthFile(urls.first().toString());
        if (urls.size() > 1)
            item.fallbackUrl = urls.at(1).toString();
        else
            item.fallbackUrl = urls.first().toString();
        // path 先记着，等实例建好、知道实例目录之后再拼绝对路径
        item.label = path;
        item.sha1 = file.value(QStringLiteral("hashes")).toObject()
                        .value(QStringLiteral("sha1")).toString();
        item.size = qint64(file.value(QStringLiteral("fileSize")).toDouble(-1));
        m_pack.files << item;
    }

    setStage(QStringLiteral("整合包 %1：%2 %3，%4 个文件")
                 .arg(m_pack.title, m_pack.minecraft,
                      m_pack.loaderKind.isEmpty() ? QStringLiteral("原版") : m_pack.loaderKind)
                 .arg(m_pack.files.size()));

    startPackInstance();
}

void VanillaKernel::startPackInstance()
{
    // 按整合包声明的依赖建实例。三种加载器各走各的已有流程。
    if (m_pack.loaderKind == QLatin1String("fabric")) {
        createFabricInstance(m_pack.minecraft, m_pack.title);
        return;
    }
    if (m_pack.loaderKind == QLatin1String("forge")
        || m_pack.loaderKind == QLatin1String("neoforge")) {
        createModdedInstance(m_pack.minecraft, m_pack.loaderKind, m_pack.title);
        return;
    }
    // 纯原版整合包
    const QString id = m_store
        ? m_store->create(m_pack.title, m_pack.minecraft, QStringLiteral("Vanilla"), m_pack.iconUrl)
        : QString();
    if (id.isEmpty())
        fail(QStringLiteral("建实例失败"));
}

void VanillaKernel::beginPackFiles()
{
    // 到这里实例已经建好了（m_pack.slug 被换成实例目录）
    if (m_pack.files.isEmpty()) {
        finishPackInstall();
        return;
    }

    const QString dir = m_pack.slug; // 复用字段存"实例目录"
    QList<DownloadItem> items;
    for (DownloadItem item : m_pack.files) {
        const QString relative = item.label;
        item.targetPath = dir + QLatin1Char('/') + relative;
        items << item;
    }

    setStage(QStringLiteral("下载整合包内容 %1 个文件").arg(items.size()));
    m_step = Step::PackFiles;
    m_downloader->start(items, 8);
}

void VanillaKernel::finishPackInstall()
{
    // 解压 overrides / client-overrides 到实例目录
    const QString dir = m_pack.slug;
    for (const QString &sub : { QStringLiteral("overrides"), QStringLiteral("client-overrides") }) {
        const QString from = m_pack.extractDir + QLatin1Char('/') + sub;
        if (!QFileInfo::exists(from))
            continue;
        // 借 zip 解压器不行（这是目录），手动递归拷
        QDirIterator it(from, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString src = it.next();
            const QString rel = src.mid(from.length() + 1);
            const QString dst = dir + QLatin1Char('/') + rel;
            QDir().mkpath(QFileInfo(dst).absolutePath());
            QFile::remove(dst);
            QFile::copy(src, dst);
        }
    }
    const QString id = m_pack.slug;
    m_pack = PendingPack{};
    refresh();
    setStage(QStringLiteral("整合包安装完成"));
    Q_EMIT instanceCreated(id);
}

QVariantList VanillaKernel::availableJava() const
{
    QVariantList out;
    for (const JavaInstall &java : m_java) {
        QVariantMap item;
        item[QStringLiteral("path")] = java.path;
        item[QStringLiteral("major")] = java.major;
        item[QStringLiteral("vendor")] = java.vendor.isEmpty() ? QStringLiteral("未知") : java.vendor;
        item[QStringLiteral("version")] = java.version;
        item[QStringLiteral("label")] = java.label();
        out << item;
    }
    return out;
}

void VanillaKernel::setInstanceJava(const QString &instanceId, const QString &javaPath)
{
    if (m_store)
        m_store->setJava(instanceId, javaPath);
}

void VanillaKernel::checkContentUpdates(const QString &instanceId)
{
    if (!m_store) {
        Q_EMIT contentUpdatesFailed(instanceId, QStringLiteral("没有实例仓库"));
        return;
    }

    // 把三个内容目录里的文件都算一遍 sha1 —— Modrinth 是按文件哈希认内容的
    QHash<QString, QString> hashToFile; // sha1 -> 文件名
    QJsonArray hashes;
    const QString base = m_store->instanceDir(instanceId);

    for (const QString &folder : { QStringLiteral("mods"), QStringLiteral("resourcepacks"),
                                   QStringLiteral("shaderpacks") }) {
        QDir dir(base + QLatin1Char('/') + folder);
        if (!dir.exists())
            continue;
        const QFileInfoList files =
            dir.entryInfoList({ QStringLiteral("*.jar"), QStringLiteral("*.zip") }, QDir::Files);
        for (const QFileInfo &info : files) {
            QFile file(info.absoluteFilePath());
            if (!file.open(QIODevice::ReadOnly))
                continue;
            QCryptographicHash hash(QCryptographicHash::Sha1);
            if (!hash.addData(&file))
                continue;
            const QString hex = QString::fromLatin1(hash.result().toHex());
            hashToFile.insert(hex, info.fileName());
            hashes.append(hex);
        }
    }

    if (hashes.isEmpty()) {
        Q_EMIT contentUpdatesReady(instanceId, QVariantList());
        return;
    }

    setStage(QStringLiteral("检查 %1 个文件有没有更新").arg(hashes.size()));

    QJsonObject body;
    body[QStringLiteral("hashes")] = hashes;
    body[QStringLiteral("algorithm")] = QStringLiteral("sha1");

    QNetworkRequest request{ QUrl(modrinthBase() + QStringLiteral("/version_files/update")) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("mcl/0.1 (github.com/cccp00-cup/mcl)"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply =
        m_net->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, instanceId, hashToFile]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT contentUpdatesFailed(instanceId,
                                        QStringLiteral("检查更新失败：%1").arg(reply->errorString()));
            return;
        }

        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        QVariantList updates;
        for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
            // 返回的是"当前 hash -> 该内容的最新版本"
            const QString fileName = hashToFile.value(it.key());
            if (fileName.isEmpty())
                continue;
            const QJsonObject version = it.value().toObject();
            const QString latest = version.value(QStringLiteral("version_number")).toString();

            // 版本号里一般带文件名特征；同名就不算有更新
            const QString newFile = version.value(QStringLiteral("files")).toArray().isEmpty()
                ? QString()
                : version.value(QStringLiteral("files")).toArray().first().toObject()
                      .value(QStringLiteral("filename")).toString();
            if (!newFile.isEmpty() && newFile == fileName)
                continue;

            QVariantMap item;
            item[QStringLiteral("fileName")] = fileName;
            item[QStringLiteral("versionNumber")] = latest;
            item[QStringLiteral("newFileName")] = newFile;
            updates << item;
        }

        setStage(updates.isEmpty() ? QStringLiteral("都是最新版")
                                   : QStringLiteral("有 %1 个可以更新").arg(updates.size()));
        Q_EMIT contentUpdatesReady(instanceId, updates);
    });
}

QVariantList VanillaKernel::instanceMods(const QString &instanceId) const
{
    QVariantList out;
    if (!m_store)
        return out;

    const QString dir = m_store->instanceDir(instanceId) + QStringLiteral("/mods");
    QDir mods(dir);
    if (!mods.exists())
        return out;

    const QFileInfoList files = mods.entryInfoList(
        { QStringLiteral("*.jar"), QStringLiteral("*.jar.disabled"), QStringLiteral("*.zip") },
        QDir::Files, QDir::Name);

    for (const QFileInfo &info : files) {
        QVariantMap item;
        const QString name = info.fileName();
        const bool disabled = name.endsWith(QLatin1String(".disabled"));
        item[QStringLiteral("fileName")] = name;
        item[QStringLiteral("displayName")] =
            disabled ? name.left(name.size() - 9) : name; // 去掉 ".disabled"
        item[QStringLiteral("enabled")] = !disabled;
        item[QStringLiteral("size")] = info.size();
        item[QStringLiteral("sizeText")] = info.size() > 1024 * 1024
            ? QStringLiteral("%1 MB").arg(info.size() / 1024.0 / 1024.0, 0, 'f', 1)
            : QStringLiteral("%1 KB").arg(qMax<qint64>(1, info.size() / 1024));
        out << item;
    }
    return out;
}

void VanillaKernel::removeInstanceMod(const QString &instanceId, const QString &fileName)
{
    if (!m_store || fileName.isEmpty())
        return;
    const QString path = m_store->instanceDir(instanceId) + QStringLiteral("/mods/") + fileName;
    if (QFile::remove(path))
        Q_EMIT logLine(QStringLiteral("[mcl] 已移除模组 %1").arg(fileName));
    else
        Q_EMIT logLine(QStringLiteral("[mcl] 移除 %1 失败").arg(fileName));
    Q_EMIT instanceModsChanged(instanceId);
}

void VanillaKernel::toggleInstanceMod(const QString &instanceId, const QString &fileName)
{
    if (!m_store || fileName.isEmpty())
        return;

    const QString dir = m_store->instanceDir(instanceId) + QStringLiteral("/mods/");
    // 停用 = 把文件名加上 .disabled（Forge/Fabric 都只加载 .jar）
    const QString target = fileName.endsWith(QLatin1String(".disabled"))
        ? dir + fileName.left(fileName.size() - 9)
        : dir + fileName + QStringLiteral(".disabled");

    if (!QFile::rename(dir + fileName, target)) {
        Q_EMIT logLine(QStringLiteral("[mcl] 切换 %1 状态失败").arg(fileName));
        return;
    }
    Q_EMIT instanceModsChanged(instanceId);
}

void VanillaKernel::openInstanceFolder(const QString &instanceId) const
{
    if (!m_store)
        return;
    const QString dir = m_store->instanceDir(instanceId);
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void VanillaKernel::openModsFolder(const QString &instanceId) const
{
    if (!m_store)
        return;
    const QString dir = m_store->instanceDir(instanceId) + QStringLiteral("/mods");
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

// 把一个 Modrinth 项目做成下载项（用某个具体版本的文件）
namespace
{
bool appendModFile(const QJsonObject &version, const QString &targetDir,
                   QList<DownloadItem> *into, QString *label)
{
    const QJsonArray files = version.value(QStringLiteral("files")).toArray();
    if (files.isEmpty())
        return false;

    // 优先取标记为 primary 的那个，没有就取第一个
    QJsonObject chosen = files.first().toObject();
    for (const QJsonValue &fv : files) {
        const QJsonObject file = fv.toObject();
        if (file.value(QStringLiteral("primary")).toBool()) {
            chosen = file;
            break;
        }
    }

    const QString name = chosen.value(QStringLiteral("filename")).toString();
    if (name.isEmpty())
        return false;

    const QString url = chosen.value(QStringLiteral("url")).toString();
    DownloadItem item;
    item.url = url.startsWith(QLatin1String("https://cdn.modrinth.com"))
        ? QStringLiteral("https://mod.mcimirror.top") + url.mid(int(sizeof("https://cdn.modrinth.com") - 1))
        : url;
    item.fallbackUrl = url;
    item.targetPath = targetDir + QLatin1Char('/') + name;
    item.sha1 = chosen.value(QStringLiteral("hashes")).toObject()
                    .value(QStringLiteral("sha1")).toString();
    item.size = qint64(chosen.value(QStringLiteral("size")).toDouble(-1));
    item.label = *label = name;
    *into << item;
    return true;
}
} // namespace

void VanillaKernel::startModDownload(const QList<DownloadItem> &items, const QString &what)
{
    if (items.isEmpty()) {
        setStage(QStringLiteral("%1：没有可下载的文件").arg(what));
        return;
    }
    setStage(items.size() > 1
                 ? QStringLiteral("下载 %1 及其依赖（%2 个文件）").arg(what).arg(items.size())
                 : QStringLiteral("下载 %1").arg(what));
    m_step = Step::None;
    m_downloader->start(items, 4);
}

void VanillaKernel::collectDependencies(const QString &instanceId, const QJsonArray &dependencies,
                                        QList<DownloadItem> *into,
                                        const QSharedPointer<QSet<QString>> &seen,
                                        const QSharedPointer<int> &pending,
                                        const QString &targetDir)
{
    // 依赖可能还有自己的依赖，所以这里按"层"递归，用 pending 计数等全部回来
    for (const QJsonValue &dv : dependencies) {
        const QJsonObject dep = dv.toObject();
        // 只要 required：optional 是可选功能，incompatible 是要主动避开的
        if (dep.value(QStringLiteral("dependency_type")).toString() != QLatin1String("required"))
            continue;

        const QString versionId = dep.value(QStringLiteral("version_id")).toString();
        const QString projectId = dep.value(QStringLiteral("project_id")).toString();
        const QString key = versionId.isEmpty() ? projectId : versionId;
        if (key.isEmpty() || seen->contains(key))
            continue;
        seen->insert(key);

        // 依赖里一般直接给 version_id，那就精确查这一个版本
        const QString url = versionId.isEmpty()
            ? modrinthBase() + QStringLiteral("/project/%1/version").arg(projectId)
            : modrinthBase() + QStringLiteral("/version/%1").arg(versionId);

        (*pending)++;
        QNetworkReply *reply = m_net->get(modrinthRequest(url));
        connect(reply, &QNetworkReply::finished, this,
                [this, reply, into, seen, pending, targetDir, versionId]() {
                    reply->deleteLater();
                    if (reply->error() == QNetworkReply::NoError) {
                        const QByteArray payload = reply->readAll();
                        QJsonObject version;
                        if (versionId.isEmpty()) {
                            const QJsonArray arr = QJsonDocument::fromJson(payload).array();
                            if (!arr.isEmpty())
                                version = arr.first().toObject();
                        } else {
                            version = QJsonDocument::fromJson(payload).object();
                        }

                        QString name;
                        if (appendModFile(version, targetDir, into, &name)) {
                            Q_EMIT logLine(QStringLiteral("[mcl] 依赖：%1").arg(name));
                            // 依赖自己的依赖
                            collectDependencies(QString(), version.value(QStringLiteral("dependencies")).toArray(),
                                                into, seen, pending, targetDir);
                        }
                    }
                    if (--(*pending) == 0)
                        startModDownload(*into, m_stage);
                });
    }
}

QString VanillaKernel::folderForKind(const QString &kind)
{
    if (kind == QLatin1String("resourcepack"))
        return QStringLiteral("resourcepacks");
    if (kind == QLatin1String("shader"))
        return QStringLiteral("shaderpacks");
    return QStringLiteral("mods");
}

void VanillaKernel::installContent(const QString &instanceId, const QString &projectId,
                                   const QString &kind, const QString &title)
{
    const QVariantMap instance = m_store ? m_store->instance(instanceId) : QVariantMap();
    if (instance.isEmpty()) {
        fail(QStringLiteral("找不到实例 %1").arg(instanceId));
        return;
    }
    const QString instanceDir = m_store->instanceDir(instanceId)
        + QLatin1Char('/') + folderForKind(kind);

    setStage(QStringLiteral("查询模组 %1").arg(title.isEmpty() ? projectId : title));
    QNetworkReply *reply = m_net->get(
        modrinthRequest(modrinthBase() + QStringLiteral("/project/%1/version").arg(projectId)));
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, projectId, title, instanceDir]() {
                reply->deleteLater();
                if (reply->error() != QNetworkReply::NoError) {
                    fail(QStringLiteral("查模组版本失败：%1").arg(reply->errorString()));
                    return;
                }

                const QJsonArray versions = QJsonDocument::fromJson(reply->readAll()).array();
                if (versions.isEmpty()) {
                    fail(QStringLiteral("这个模组没有可用版本"));
                    return;
                }

                const QJsonObject version = versions.first().toObject();
                const QString label = title.isEmpty() ? projectId : title;

                QList<DownloadItem> items;
                QString name;
                if (!appendModFile(version, instanceDir, &items, &name)) {
                    fail(QStringLiteral("这个模组版本没有可下载的文件"));
                    return;
                }

                // 同一次安装里别把同一个依赖拉两遍
                auto seen = QSharedPointer<QSet<QString>>::create();
                seen->insert(projectId);
                auto pending = QSharedPointer<int>::create(0);
                collectDependencies(instanceDir.isEmpty() ? QString() : QString(),
                                    version.value(QStringLiteral("dependencies")).toArray(),
                                    &items, seen, pending, instanceDir);

                if (*pending == 0) {
                    // 没有依赖，直接下
                    startModDownload(items, label);
                } else {
                    setStage(QStringLiteral("%1：还要解析 %2 个依赖").arg(label).arg(*pending));
                }
            });
}

void VanillaKernel::afterInstanceReady(const QString &instanceId, const QString &instanceName)
{
    if (m_pack.active) {
        // 整合包流程：实例建好了，把它自己的目录记下来，接着下内容
        m_pack.slug = m_store ? m_store->instanceDir(instanceId) : QString();
        setStage(QStringLiteral("已创建实例 %1，开始装内容").arg(instanceName));
        beginPackFiles();
        return;
    }

    setStage(QStringLiteral("已创建 %1").arg(instanceName));
    Q_EMIT instanceCreated(instanceId);
}

void VanillaKernel::createModdedInstance(const QString &gameVersion, const QString &loader,
                                         const QString &name)
{
    const ModLoader::Kind kind = ModLoader::fromId(loader);
    if (kind != ModLoader::Kind::Forge && kind != ModLoader::Kind::NeoForge) {
        fail(QStringLiteral("mcl 目前只支持 Forge 与 NeoForge（收到 %1）").arg(loader));
        return;
    }
    if (m_downloader->isRunning()) {
        fail(QStringLiteral("还有下载没结束，稍后再试"));
        return;
    }

    m_pendingLoader = kind;
    m_pendingGameVersion = gameVersion;
    m_pendingLoaderName = name;
    setStage(QStringLiteral("准备安装 %1（先确保原版就绪）").arg(ModLoader::displayName(kind)));

    // installer 要给客户端打补丁，所以原版必须先装好。
    // 已经装过的话这一步几乎瞬间完成（文件都在，只做校验）。
    startInstall(gameVersion, false, QString());
}

void VanillaKernel::fetchLoaderMetadata()
{
    const ModLoader::Kind kind = m_pendingLoader;
    setStage(QStringLiteral("查询 %1 的版本").arg(ModLoader::displayName(kind)));

    QNetworkRequest request{ QUrl(ModLoader::metadataUrl(kind)) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));

    m_step = Step::LoaderMetadata;
    QNetworkReply *reply = m_net->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, kind]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("取 %1 版本列表失败：%2")
                     .arg(ModLoader::displayName(kind), reply->errorString()));
            return;
        }

        const QString version =
            ModLoader::pickVersion(kind, reply->readAll(), m_pendingGameVersion);
        if (version.isEmpty()) {
            fail(QStringLiteral("%1 没有为 %2 提供版本")
                     .arg(ModLoader::displayName(kind), m_pendingGameVersion));
            return;
        }
        m_loaderVersion = version;
        downloadLoaderInstaller();
    });
}

void VanillaKernel::downloadLoaderInstaller()
{
    const ModLoader::Kind kind = m_pendingLoader;
    const QString url = ModLoader::installerUrl(kind, m_pendingGameVersion, m_loaderVersion);
    m_loaderInstallerPath = dataDir() + QStringLiteral("/installers/")
        + url.section(QLatin1Char('/'), -1);

    setStage(QStringLiteral("下载 %1 installer（%2）")
                 .arg(ModLoader::displayName(kind), m_loaderVersion));

    m_step = Step::LoaderInstaller;
    DownloadItem item;
    item.url = Mirror::rewrite(url);
    item.fallbackUrl = url;
    item.targetPath = m_loaderInstallerPath;
    item.label = QStringLiteral("%1 installer").arg(ModLoader::displayName(kind));
    m_downloader->start({ item }, 1);
}

void VanillaKernel::runLoaderInstaller()
{
    // 新版 Forge / NeoForge 的 installer 都要 Java 17+
    const JavaInstall java = JavaLocator::findBest(17);
    if (!java.isValid()) {
        fail(QStringLiteral("找不到 Java 17 及以上 —— installer 需要它"));
        return;
    }

    // installer 会检查这个文件，没有会直接报错退出
    const QString profilesPath = dataDir() + QStringLiteral("/launcher_profiles.json");
    if (!QFileInfo::exists(profilesPath)) {
        QFile profiles(profilesPath);
        if (profiles.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            profiles.write("{\"profiles\":{},\"settings\":{},\"version\":3}");
            profiles.close();
        }
    }

    setStage(QStringLiteral("运行 %1 installer（要打补丁，可能要一会儿）")
                 .arg(ModLoader::displayName(m_pendingLoader)));

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(dataDir());
    m_process->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines)
            Q_EMIT logLine(QStringLiteral("[installer] %1").arg(line.trimmed()));
    });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) {
        m_process->deleteLater();
        m_process = nullptr;
        if (exitCode != 0) {
            fail(QStringLiteral("installer 退出码 %1").arg(exitCode));
            return;
        }
        finishModdedInstance();
    });

    // installer 会自己联网下东西（用的是它内置的地址，不走我们的镜像）
    m_process->start(java.path,
                     { QStringLiteral("-jar"), m_loaderInstallerPath,
                       QStringLiteral("--installClient"), dataDir() });
    if (!m_process->waitForStarted(15000))
        fail(QStringLiteral("installer 起不来：%1").arg(m_process->errorString()));
}

void VanillaKernel::finishModdedInstance()
{
    // installer 会在 versions/ 下生成形如 "1.20.1-forge-47.4.9" 的目录，
    // 挑出名字里带加载器和游戏版本、且最新的那个
    const QString needle = ModLoader::displayName(m_pendingLoader).toLower();
    QDir versions(versionsDir());
    QString found;
    QDateTime newest;

    const QStringList dirs =
        versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    for (const QString &dir : dirs) {
        const QString lower = dir.toLower();
        if (!lower.contains(needle) || !lower.contains(m_pendingGameVersion))
            continue;
        if (!QFileInfo::exists(versionJsonPath(dir)))
            continue;
        const QDateTime when = QFileInfo(versions.filePath(dir)).lastModified();
        if (found.isEmpty() || when > newest) {
            found = dir;
            newest = when;
        }
    }

    if (found.isEmpty()) {
        fail(QStringLiteral("installer 跑完了，但没找到它生成的版本描述"));
        return;
    }

    const QString loaderName = ModLoader::displayName(m_pendingLoader);
    const QString instanceName =
        m_pendingLoaderName.isEmpty()
            ? QStringLiteral("%1 %2 · %3").arg(loaderName, m_pendingGameVersion, m_loaderVersion)
            : m_pendingLoaderName;

    const QString instanceId =
        m_store ? m_store->create(instanceName, found, loaderName, m_pack.iconUrl) : QString();

    m_pendingLoader = ModLoader::Kind::Vanilla;
    refresh();
    afterInstanceReady(instanceId, instanceName);
}

namespace
{
// Fabric Meta 的 profile 接口要求带上加载器版本，所以得先问一句
// "这个游戏版本能用哪些加载器"。两个 URL 都在这儿，免得散落各处。
QString fabricLoaderListUrl(const QString &gameVersion)
{
    return Mirror::rewrite(
        QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1").arg(gameVersion));
}

QString fabricProfileUrl(const QString &gameVersion, const QString &loaderVersion)
{
    return Mirror::rewrite(
        QStringLiteral("https://meta.fabricmc.net/v2/versions/loader/%1/%2/profile/json")
            .arg(gameVersion, loaderVersion));
}

QNetworkRequest fabricRequest(const QString &url)
{
    QNetworkRequest request{ QUrl(url) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("mcl/0.1"));
    return request;
}
} // namespace

void VanillaKernel::createFabricInstance(const QString &gameVersion, const QString &name)
{
    setStage(QStringLiteral("获取 Fabric %1 的加载器列表").arg(gameVersion));

    QNetworkReply *listReply = m_net->get(fabricRequest(fabricLoaderListUrl(gameVersion)));
    connect(listReply, &QNetworkReply::finished, this, [this, listReply, gameVersion, name]() {
        listReply->deleteLater();
        if (listReply->error() != QNetworkReply::NoError) {
            fail(QStringLiteral("取 Fabric 加载器列表失败：%1").arg(listReply->errorString()));
            return;
        }

        const QJsonArray loaders = QJsonDocument::fromJson(listReply->readAll()).array();
        if (loaders.isEmpty()) {
            fail(QStringLiteral("Fabric 没有为 %1 提供加载器").arg(gameVersion));
            return;
        }
        const QString loaderVersion =
            loaders.first().toObject().value(QStringLiteral("loader")).toObject()
                .value(QStringLiteral("version")).toString();
        if (loaderVersion.isEmpty()) {
            fail(QStringLiteral("Fabric 返回的加载器信息不完整"));
            return;
        }

        setStage(QStringLiteral("获取 Fabric %1 的版本描述").arg(loaderVersion));
        QNetworkReply *profileReply =
            m_net->get(fabricRequest(fabricProfileUrl(gameVersion, loaderVersion)));

        connect(profileReply, &QNetworkReply::finished, this,
                [this, profileReply, gameVersion, loaderVersion, name]() {
                    profileReply->deleteLater();
                    if (profileReply->error() != QNetworkReply::NoError) {
                        fail(QStringLiteral("取 Fabric 描述失败：%1")
                                 .arg(profileReply->errorString()));
                        return;
                    }

                    const QByteArray payload = profileReply->readAll();
                    const QJsonObject root = QJsonDocument::fromJson(payload).object();
                    const QString id = root.value(QStringLiteral("id")).toString();
                    const QString parent = root.value(QStringLiteral("inheritsFrom")).toString();
                    if (id.isEmpty() || parent.isEmpty()) {
                        fail(QStringLiteral("Fabric 返回的描述不完整"));
                        return;
                    }

                    // 落成一份普通的版本描述放进 versions/<id>/ —— 之后它和原版版本
                    // 一视同仁，启动时由 parseVersionJson 顺着 inheritsFrom 合并
                    const QString dir = versionsDir() + QLatin1Char('/') + id;
                    QDir().mkpath(dir);
                    QFile file(dir + QLatin1Char('/') + id + QStringLiteral(".json"));
                    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                        fail(QStringLiteral("写不了 %1 的版本描述").arg(id));
                        return;
                    }
                    file.write(payload);
                    file.close();

                    const QString instanceName = name.isEmpty()
                        ? QStringLiteral("Fabric %1 · %2").arg(loaderVersion, gameVersion)
                        : name;
                    const QString instanceId =
                        m_store ? m_store->create(instanceName, id, QStringLiteral("Fabric"))
                                : QString();
                    refresh();
                    afterInstanceReady(instanceId, instanceName);
                });
    });
}

void VanillaKernel::renameInstance(const QString &instanceId, const QString &name)
{
    if (m_store)
        m_store->rename(instanceId, name);
}

void VanillaKernel::removeInstance(const QString &instanceId)
{
    if (m_store)
        m_store->remove(instanceId);
}

void VanillaKernel::moveInstance(const QString &instanceId, int newIndex)
{
    if (m_store)
        m_store->move(instanceId, newIndex);
}

void VanillaKernel::pinInstance(const QString &instanceId, bool pinned)
{
    if (m_store)
        m_store->setPinned(instanceId, pinned);
}

QString VanillaKernel::instanceDir(const QString &instanceId) const
{
    return m_store ? m_store->instanceDir(instanceId) : QString();
}

QVariantMap VanillaKernel::instanceInfo(const QString &instanceId) const
{
    if (!m_store)
        return {};
    return m_store->instance(instanceId);
}

QString VanillaKernel::instanceName(const QString &instanceId) const
{
    if (!m_store)
        return instanceId;
    const QVariantMap item = m_store->instance(instanceId);
    const QString name = item.value(QStringLiteral("name")).toString();
    return name.isEmpty() ? instanceId : name;
}

// ——————————————————————————————————————————————— 启动流程

void VanillaKernel::launch(const QString &instanceId)
{
    launchOffline(instanceId, QStringLiteral("Player"));
}

void VanillaKernel::launchOffline(const QString &instanceId, const QString &playerName)
{
    startInstall(instanceId, true, playerName);
}

void VanillaKernel::install(const QString &versionId)
{
    startInstall(versionId, false, QString());
}

void VanillaKernel::startInstall(const QString &instanceId, bool thenLaunch,
                                 const QString &playerName)
{
    // 解析实例 → 版本 + 游戏目录。找不到同名实例时把参数当版本号用
    // （命令行 --launch 1.21 走的就是这条路），那时用共享目录。
    QString versionId = instanceId;
    QString gameDir;
    if (m_store) {
        const QVariantMap item = m_store->instance(instanceId);
        if (!item.isEmpty()) {
            versionId = item.value(QStringLiteral("versionId")).toString();
            gameDir = m_store->instanceDir(instanceId);
        }
    }
    if (versionId.isEmpty()) {
        Q_EMIT launchFailed(instanceId, QStringLiteral("找不到实例 %1").arg(instanceId));
        return;
    }

    const QString player = playerName.isEmpty() ? QStringLiteral("Player") : playerName;

    if (m_downloader->isRunning()) {
        // 不要硬失败：这一批下载跑完后会回头接着装（见 onDownloadFinished）
        m_activeInstanceId = instanceId;
        m_activeVersionId = versionId;
        m_activeGameDir = gameDir;
        m_pendingPlayer = player;
        m_launchAfterInstall = thenLaunch;
        Q_EMIT logLine(QStringLiteral("[mcl] 已有下载在进行，%1 排队等它结束").arg(instanceId));
        return;
    }
    if (isRunning(instanceId)) {
        Q_EMIT launchFailed(instanceId, QStringLiteral("这个实例已经在运行了"));
        return;
    }

    m_activeInstanceId = instanceId;
    m_activeVersionId = versionId;
    m_activeGameDir = gameDir;
    m_pendingPlayer = player;
    m_launchAfterInstall = thenLaunch;

    QDir().mkpath(versionsDir() + QLatin1Char('/') + versionId);
    if (!gameDir.isEmpty())
        QDir().mkpath(gameDir);
    resumeInstall();
}

// 确定这个版本接下来该干什么：已经装好了就直接下文件，还没描述就先拿描述
void VanillaKernel::resumeInstall()
{
    if (m_activeVersionId.isEmpty())
        return;

    if (!QFileInfo::exists(versionJsonPath(m_activeVersionId))) {
        // 注意：这里**不能**直接调 ensureManifest() —— 清单加载完之后它会回头调
        // resumeInstall()，两边互相调用就成死循环了。先确保清单在手，再直接去下描述。
        if (!m_manifestLoaded) {
            ensureManifest();
            return;
        }
        ensureVersionJson(m_activeVersionId);
        return;
    }

    // 模组加载器版本继承了原版：先把父链上缺的描述补下来，再谈合并
    const QString missing = findMissingAncestor(m_activeVersionId);
    if (!missing.isEmpty()) {
        if (m_manifestLoaded) {
            ensureVersionJson(missing);
        } else {
            ensureManifest();
        }
        return;
    }

    QString error;
    if (!parseVersionJson(&error)) {
        Q_EMIT launchFailed(m_activeInstanceId, error);
        return;
    }
    beginVersionFiles();
}

void VanillaKernel::ensureManifest()
{
    if (QFileInfo::exists(manifestPath())) {
        QFile file(manifestPath());
        if (file.open(QIODevice::ReadOnly)) {
            m_manifest = McVersion::parseManifest(QJsonDocument::fromJson(file.readAll()).object());
            m_manifestLoaded = !m_manifest.isEmpty();
            m_versionsCache.clear();
            Q_EMIT manifestChanged();
        }
    }
    if (m_manifestLoaded) {
        // 不能写死 ensureVersionJson(m_activeVersionId) —— 走到这里可能是为了
        // 补某个**父版本**的描述（模组加载器继承）。交回 resumeInstall 重新判断。
        if (!m_activeVersionId.isEmpty())
            resumeInstall();
        return;
    }

    setStage(QStringLiteral("获取官方版本清单"));
    m_step = Step::Manifest;
    DownloadItem item;
    item.url = Mirror::rewrite(QString::fromLatin1(kManifestUrl));
    item.fallbackUrl = QString::fromLatin1(kManifestUrl);
    item.targetPath = manifestPath();
    item.label = QStringLiteral("version_manifest_v2.json");
    m_downloader->start({ item }, 1);
}

void VanillaKernel::ensureVersionJson(const QString &versionId)
{
    QString url;
    for (const McManifestEntry &entry : m_manifest) {
        if (entry.id == versionId) {
            url = entry.url;
            break;
        }
    }
    if (url.isEmpty()) {
        fail(QStringLiteral("版本清单里找不到 %1").arg(versionId));
        return;
    }

    setStage(QStringLiteral("获取 %1 的版本描述").arg(versionId));
    m_step = Step::VersionJson;
    DownloadItem item;
    item.url = Mirror::rewrite(url);
    item.fallbackUrl = url;
    item.targetPath = versionJsonPath(versionId);
    item.label = versionId + QStringLiteral(".json");
    m_downloader->start({ item }, 1);
}

// 模组加载器的版本描述继承了原版，所以启动前要把整条链上的描述都备齐
QString VanillaKernel::findMissingAncestor(const QString &versionId) const
{
    QString current = versionId;
    for (int depth = 0; depth < 8; ++depth) {
        QFile file(versionJsonPath(current));
        if (!file.open(QIODevice::ReadOnly))
            return current; // 这一个就缺
        const McVersionInfo info =
            McVersion::parseVersion(QJsonDocument::fromJson(file.readAll()).object());
        if (info.inheritsFrom.isEmpty())
            return {}; // 链到头了
        current = info.inheritsFrom;
    }
    return {};
}

bool VanillaKernel::parseVersionJson(QString *error)
{
    QFile file(versionJsonPath(m_activeVersionId));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("读不到 %1").arg(versionJsonPath(m_activeVersionId));
        return false;
    }
    McVersionInfo info = McVersion::parseVersion(QJsonDocument::fromJson(file.readAll()).object());
    if (!info.isValid()) {
        if (error)
            *error = QStringLiteral("%1 的版本描述不完整").arg(m_activeVersionId);
        return false;
    }

    // 顺着 inheritsFrom 一路往上合并（Fabric / Forge 的描述都只是薄壳）
    int depth = 0;
    while (!info.inheritsFrom.isEmpty() && depth++ < 8) {
        const QString parentId = info.inheritsFrom;
        QFile parentFile(versionJsonPath(parentId));
        if (!parentFile.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("缺少父版本 %1 的版本描述").arg(parentId);
            return false;
        }
        const McVersionInfo parent =
            McVersion::parseVersion(QJsonDocument::fromJson(parentFile.readAll()).object());
        info = McVersion::mergeInherited(info, parent);
    }

    m_version = info;
    return true;
}

void VanillaKernel::beginVersionFiles()
{
    const QList<DownloadItem> items = LaunchPlan::collectVersionFiles(
        m_version, versionsDir(), librariesDir(), assetsDir());

    setStage(QStringLiteral("%1：需要 %2 个文件").arg(m_activeVersionId).arg(items.size()));
    m_step = Step::VersionFiles;
    m_downloader->start(items, 12);
}

void VanillaKernel::beginAssets()
{
    const QString indexPath =
        assetsDir() + QStringLiteral("/indexes/") + m_version.assetIndex.id + QStringLiteral(".json");
    const QList<DownloadItem> items = LaunchPlan::collectAssets(indexPath, assetsDir());

    // MCL_SKIP_ASSETS=1 用于开发时跳过几百 MB 的资源下载，直接验证启动链路
    if (items.isEmpty() || qEnvironmentVariableIsSet("MCL_SKIP_ASSETS")) {
        Q_EMIT logLine(QStringLiteral("[mcl] 跳过资源下载（%1 个对象）").arg(items.size()));
        extractNatives();
        return;
    }

    setStage(QStringLiteral("下载资源 %1 个").arg(items.size()));
    m_step = Step::Assets;
    m_downloader->start(items, 16);
}

void VanillaKernel::extractNatives()
{
    const QString target = nativesDirFor(m_activeVersionId);
    QDir(target).removeRecursively();
    QDir().mkpath(target);

    const QMap<QString, bool> features = McVersion::defaultFeatures();
    int extracted = 0;
    for (const McLibrary &lib : m_version.libraries) {
        if (!lib.isNative || !lib.appliesTo(features) || lib.path.isEmpty())
            continue;
        const QString jar = librariesDir() + QLatin1Char('/') + lib.path;
        if (!QFileInfo::exists(jar))
            continue;
        QString error;
        if (!ZipExtract::extract(jar, target, lib.extractExclude, &error)) {
            Q_EMIT logLine(QStringLiteral("[mcl] 解压 %1 失败：%2").arg(lib.name, error));
            continue;
        }
        ++extracted;
    }
    setStage(QStringLiteral("解压 natives（%1 个包）").arg(extracted));

    if (m_launchAfterInstall) {
        startProcess();
        return;
    }

    // 只装不启动：如果这次是"建模组实例"的前半程，接着去装加载器
    if (m_pendingLoader != ModLoader::Kind::Vanilla) {
        fetchLoaderMetadata();
        return;
    }

    m_step = Step::None;
    refresh();
    Q_EMIT launchFinished(m_activeInstanceId, 0);
}

void VanillaKernel::startProcess()
{
    // 1.16 及更早的 version.json 没有 javaVersion 字段，那时的 Minecraft 一律要 Java 8
    const int requiredJava = m_version.javaMajor > 0 ? m_version.javaMajor : 8;

    // 玩家在实例设置里指定过就用他指定的；没指定（或指定的那个已经不存在了）
    // 才按版本要求自动挑一个。
    JavaInstall java;
    QString pinnedJava;
    if (m_store && !m_activeInstanceId.isEmpty()) {
        const QVariantMap instance = m_store->instance(m_activeInstanceId);
        pinnedJava = instance.value(QStringLiteral("javaPath")).toString();
    }
    if (!pinnedJava.isEmpty()) {
        java = JavaLocator::probe(pinnedJava);
        if (java.isValid()) {
            Q_EMIT logLine(QStringLiteral("[mcl] 使用实例指定的 %1").arg(java.label()));
        } else {
            Q_EMIT logLine(QStringLiteral("[mcl] 指定的 Java（%1）不可用，改回自动挑选")
                               .arg(pinnedJava));
        }
    }
    if (!java.isValid())
        java = JavaLocator::findBest(requiredJava);

    if (!java.isValid()) {
        fail(QStringLiteral("找不到可用的 Java —— %1 需要 Java %2")
                 .arg(m_activeVersionId)
                 .arg(requiredJava));
        return;
    }
    if (java.major < requiredJava) {
        Q_EMIT logLine(QStringLiteral("[mcl] 注意：%1 需要 Java %2，当前用的是 Java %3")
                           .arg(m_activeVersionId)
                           .arg(requiredJava)
                           .arg(java.major));
    }

    // 账户凭据：登录了就发真实的，没登录就用离线身份
    const QString playerName = m_account && m_account->signedIn() ? m_account->playerName()
                                                                  : m_pendingPlayer;
    const QString playerUuid = m_account && m_account->signedIn()
        ? m_account->uuid()
        : LaunchPlan::offlineUuid(playerName);
    const QString accessToken = m_account ? m_account->accessToken() : QStringLiteral("0");
    const QString userType = m_account ? m_account->userType() : QStringLiteral("legacy");

    const QStringList args = LaunchPlan::buildArguments(
        m_version, playerName, playerUuid, accessToken, userType, mergedDir(), assetsDir(),
        nativesDirFor(m_activeVersionId), librariesDir(), clientJarPath(m_activeVersionId),
        m_memoryMb);

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(mergedDir());
    m_process->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString text = QString::fromUtf8(m_process->readAllStandardOutput());
        const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines)
            Q_EMIT logLine(line);
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        if (m_process)
            fail(QStringLiteral("启动失败：%1").arg(m_process->errorString()));
    });
    connect(m_process, &QProcess::finished, this,
            [this](int exitCode, QProcess::ExitStatus) {
                const QString id = m_activeInstanceId;
                Q_EMIT logLine(QStringLiteral("[mcl] 游戏进程结束，退出码 %1").arg(exitCode));
                m_runningInstanceId.clear();
                m_step = Step::None;
                Q_EMIT launchFinished(id, exitCode);
                refresh();
                if (m_process) {
                    m_process->deleteLater();
                    m_process = nullptr;
                }
            });

    // 把完整命令打进日志 —— 出问题时这是第一手材料
    Q_EMIT logLine(QStringLiteral("[mcl] %1").arg(java.path));
    Q_EMIT logLine(QStringLiteral("[mcl] 工作目录 %1").arg(mergedDir()));
    Q_EMIT logLine(QStringLiteral("[mcl] 参数 %1").arg(args.join(QLatin1Char(' '))));

    setStage(QStringLiteral("启动 %1（Java %2）").arg(m_activeVersionId).arg(java.major));
    m_process->start(java.path, args);

    if (!m_process->waitForStarted(15000)) {
        fail(QStringLiteral("java 没能启动：%1").arg(m_process->errorString()));
        return;
    }

    m_runningInstanceId = m_activeInstanceId;
    Q_EMIT launchStarted(m_activeInstanceId);
    refresh();
}

void VanillaKernel::onDownloadFinished(bool ok, const QString &error)
{
    if (!ok) {
        fail(error);
        return;
    }

    const Step step = m_step;
    m_step = Step::None;

    // Step::None 意味着这次下载不是启动流程发起的（比如后台预取），
    // 但可能有排队的启动请求在等它
    if (step == Step::None) {
        if (!m_activeVersionId.isEmpty())
            resumeInstall();
        return;
    }

    switch (step) {
    case Step::Manifest: {
        QFile file(manifestPath());
        if (!file.open(QIODevice::ReadOnly)) {
            fail(QStringLiteral("读不到版本清单"));
            return;
        }
        m_manifest = McVersion::parseManifest(QJsonDocument::fromJson(file.readAll()).object());
        m_manifestLoaded = !m_manifest.isEmpty();
        if (!m_manifestLoaded) {
            fail(QStringLiteral("版本清单解析失败"));
            return;
        }
        Q_EMIT manifestChanged();
        refresh();
        // 交回 resumeInstall 重新判断该下哪个版本的描述 ——
        // 需要补的可能是**父版本**（模组加载器继承），不一定是正在启动的那个
        if (!m_activeVersionId.isEmpty())
            resumeInstall();
        break;
    }
    case Step::VersionJson:
        // 可能还有别的祖先要下，也可能已经齐了 —— resumeInstall 自己会判断
        resumeInstall();
        break;
    case Step::VersionFiles:
        beginAssets();
        break;
    case Step::Assets:
        extractNatives();
        break;
    case Step::LoaderInstaller:
        runLoaderInstaller();
        break;
    case Step::PackDownload:
        downloadPack();
        break;
    case Step::PackFiles:
        finishPackInstall();
        break;
    case Step::LoaderMetadata:
    case Step::None:
        break;
    }
}
