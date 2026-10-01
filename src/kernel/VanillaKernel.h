#pragma once

#include "Downloader.h"   // PendingPack 里要用 DownloadItem
#include "JavaLocator.h"
#include "McKernel.h"
#include "McVersion.h"
#include "ModLoader.h"

#include <QJsonArray>
#include <QList>
#include <QSharedPointer>
#include <QString>

class AccountManager;
class InstanceStore;
class QNetworkAccessManager;
class QProcess;

// 自研的 Vanilla 内核：mcl 自己完成从"版本清单"到"起进程"的整条链路。
//
// 不依赖 PrismLauncher，也不依赖任何第三方解压/网络库 —— 只用 Qt 和 zlib。
// 启动流程（每一步都可能失败，失败就带着原因回到界面）：
//
//   1. 确保 version_manifest_v2.json 在本地（没有就下）
//   2. 确保 versions/<id>/<id>.json 在本地（没有就按清单里的地址下）
//   3. 下载客户端 jar + 全部适用平台的库 + asset index
//   4. 读 asset index，下载全部资源对象（这一步最慢，几百 MB）
//   5. 把 natives 包解压到 natives/<id>/
//   6. 挑一个满足 javaVersion.majorVersion 的 Java
//   7. 拼出完整 java 命令行并启动，stdout/stderr 转发给界面
class VanillaKernel : public McKernel
{
    Q_OBJECT

    Q_PROPERTY(QString dataPath READ dataPath CONSTANT)
    Q_PROPERTY(QString javaInfo READ javaInfo NOTIFY javaInfoChanged)
    Q_PROPERTY(QString currentStage READ currentStage NOTIFY stageChanged)
    // 清单里最新的几个正式版，供“新建实例”选版本
    Q_PROPERTY(QVariantList availableVersions READ availableVersions NOTIFY manifestChanged)

public:
    VanillaKernel(InstanceStore *store, AccountManager *account, QObject *parent = nullptr);
    ~VanillaKernel() override;

    QString name() const override;
    QString unavailableReason() const override;
    bool isRunning(const QString &instanceId) const override;

    void refresh() override;
    void launch(const QString &instanceId) override;
    void launchOffline(const QString &instanceId, const QString &playerName) override;

    QString dataPath() const;
    QString javaInfo() const;
    QString currentStage() const { return m_stage; }
    QVariantList availableVersions() const;

    // 只下载不启动（界面上"安装"用）
    Q_INVOKABLE void install(const QString &versionId);

    // 实例增删改（转发给 InstanceStore，让界面只认 kernel 一个入口）
    Q_INVOKABLE QString createInstance(const QString &versionId, const QString &name = QString());
    // 建一个带 Fabric 加载器的实例（向 Fabric Meta 取 profile，落成一份 inheritsFrom 原版的版本描述）
    Q_INVOKABLE void createFabricInstance(const QString &gameVersion,
                                          const QString &name = QString());

    // 建 Forge / NeoForge 实例。这两个**必须跑 installer**（安装过程含打补丁、
    // 生成 patched jar、注入 processors），所以流程是：
    //   确保原版装好 → 下 installer → 跑 installer 装到共享目录 → 捡它生成的版本描述
    Q_INVOKABLE void createModdedInstance(const QString &gameVersion, const QString &loader,
                                          const QString &name = QString());

    // 从 Modrinth 装整合包（.mrpack）：
    //   取最新版本 → 下 .mrpack → 解压读 modrinth.index.json
    //   → 按它声明的加载器建实例 → 下载它列出的全部文件 → 解压 overrides
    Q_INVOKABLE void installModpack(const QString &projectId, const QString &title = QString());

    // 把一个 Modrinth 模组装进指定实例的 mods/
    // 把 Modrinth 上的内容装进实例。kind 决定落哪个目录：
    //   "mod" → mods/   "resourcepack" → resourcepacks/   "shader" → shaderpacks/
    Q_INVOKABLE void installContent(const QString &instanceId, const QString &projectId,
                                    const QString &kind = QStringLiteral("mod"),
                                    const QString &title = QString());
    Q_INVOKABLE void installMod(const QString &instanceId, const QString &projectId,
                                const QString &title = QString())
    {
        installContent(instanceId, projectId, QStringLiteral("mod"), title);
    }

    // 装模组时顺带把它声明"必需"的依赖也拉下来（不含 optional / incompatible）
    void collectDependencies(const QString &instanceId, const QJsonArray &dependencies,
                             QList<DownloadItem> *into,
                             const QSharedPointer<QSet<QString>> &seen,
                             const QSharedPointer<int> &pending,
                             const QString &targetDir);
    // 内容类型 → 实例内的子目录
    static QString folderForKind(const QString &kind);
    void startModDownload(const QList<DownloadItem> &items, const QString &what);

    // —— 实例本地模组操作
    // 扫描实例的 mods/ 目录（.jar 和 .jar.disabled 都算，后者是"已停用"）
    Q_INVOKABLE QVariantList instanceMods(const QString &instanceId) const;
    Q_INVOKABLE void removeInstanceMod(const QString &instanceId, const QString &fileName);
    Q_INVOKABLE void toggleInstanceMod(const QString &instanceId, const QString &fileName);
    // 用系统文件管理器打开实例目录 / mods 目录
    Q_INVOKABLE void openInstanceFolder(const QString &instanceId) const;
    Q_INVOKABLE void openModsFolder(const QString &instanceId) const;

    // 检查实例里的模组有没有新版本（Modrinth 的 /version_files/update）。
    // 结果用 contentUpdatesReady 发回来：{ fileName -> { versionNumber, downloadUrl } }
    Q_INVOKABLE void checkContentUpdates(const QString &instanceId);

    // 本机扫到的 Java（含厂商），给实例设置里的下拉用
    Q_INVOKABLE QVariantList availableJava() const;
    Q_INVOKABLE void setInstanceJava(const QString &instanceId, const QString &javaPath);
    Q_INVOKABLE void renameInstance(const QString &instanceId, const QString &name);
    Q_INVOKABLE void removeInstance(const QString &instanceId);
    Q_INVOKABLE void moveInstance(const QString &instanceId, int newIndex);
    Q_INVOKABLE void pinInstance(const QString &instanceId, bool pinned);
    Q_INVOKABLE QString instanceDir(const QString &instanceId) const;
    Q_INVOKABLE QString instanceName(const QString &instanceId) const;
    Q_INVOKABLE QVariantMap instanceInfo(const QString &instanceId) const;

Q_SIGNALS:
    void javaInfoChanged();
    // 某个实例的 mods/ 目录变了（增删、启用停用），界面据此刷新列表
    void instanceModsChanged(const QString &instanceId);
    // 某个实例的内容更新检查结果（fileName -> 新版本信息）
    void contentUpdatesReady(const QString &instanceId, const QVariantList &updates);
    void contentUpdatesFailed(const QString &instanceId, const QString &error);
    // 新实例建好了（界面收到后可以翻出启动台）
    void instanceCreated(const QString &instanceId);
    // 版本清单加载完成（界面据此刷新可安装列表）
    void manifestChanged();

private:
    // 下载流程所处的阶段，onDownloadFinished 靠它决定下一步
    enum class Step { None, Manifest, VersionJson, VersionFiles, Assets, LoaderMetadata, LoaderInstaller,
                      PackVersion, PackDownload, PackFiles };

    // 路径
    QString dataDir() const;
    QString mergedDir() const; // 游戏运行目录
    QString versionsDir() const;
    QString librariesDir() const;
    QString assetsDir() const;
    QString nativesDirFor(const QString &versionId) const;
    QString versionJsonPath(const QString &versionId) const;
    QString clientJarPath(const QString &versionId) const;
    QString manifestPath() const;

    void setStage(const QString &text);
    void fail(const QString &error);
    void detectJava();

    void startInstall(const QString &versionId, bool thenLaunch, const QString &playerName);
    void resumeInstall();      // 断点/排队后继续：决定是接着下载还是直接下载文件
    void prefetchManifest();   // 后台预取清单，不占用下载器
    void ensureManifest();
    void ensureVersionJson(const QString &versionId);
    // 顺着 inheritsFrom 往上找第一个还没下载的版本描述（都齐了返回空）
    QString findMissingAncestor(const QString &versionId) const;
    bool parseVersionJson(QString *error);
    void beginVersionFiles();
    void beginAssets();
    void extractNatives();
    void startProcess();
    void onDownloadFinished(bool ok, const QString &error);

    // —— Forge / NeoForge 的安装链
    void fetchLoaderMetadata();
    void downloadLoaderInstaller();
    void runLoaderInstaller();
    void finishModdedInstance();

    // —— 整合包
    void fetchPackVersion(const QString &projectId, const QString &title, int attempt = 1);
    void downloadPack();
    void unpackAndPlanPack();
    void startPackInstance();
    void beginPackFiles();
    void finishPackInstall();
    // 实例建好之后的统一出口：如果是整合包流程就接着装内容，否则通知外部
    void afterInstanceReady(const QString &instanceId, const QString &instanceName);

    // 待处理的整合包（下载/解压完之后靠它继续）
    struct PendingPack
    {
        QString projectId;
        QString title;
        QString slug;
        QString archivePath;   // 下载下来的 .mrpack
        QString extractDir;    // 解压出来的目录
        QString iconUrl;       // 整合包在 Modrinth 上的图标
        QString minecraft;     // 依赖里声明的原版版本
        QString loaderKind;    // "fabric" / "forge" / "neoforge" / ""（纯原版）
        QString loaderVersion;
        QList<DownloadItem> files;  // modrinth.index.json 里列的文件
        bool active = false;
    };
    PendingPack m_pack;

    QNetworkAccessManager *m_net = nullptr;
    Downloader *m_downloader = nullptr;
    InstanceStore *m_store = nullptr;
    AccountManager *m_account = nullptr;
    QProcess *m_process = nullptr;

    QList<McManifestEntry> m_manifest;
    bool m_manifestLoaded = false;
    // availableVersions() 的结果缓存。QML 对 var 属性会反复求值，
    // 不缓存的话每读一次就要重建 900+ 个 QVariantMap —— 实测能吃掉十几秒 CPU。
    mutable QVariantList m_versionsCache;

    Step m_step = Step::None;
    QString m_activeInstanceId; // 实例 id（界面用的是这个）
    QString m_activeVersionId;  // 实例对应的版本 id（下载/启动用的是这个）
    QString m_activeGameDir;    // 实例自己的游戏目录
    QString m_pendingPlayer;
    bool m_launchAfterInstall = false;
    McVersionInfo m_version;

    // 待安装的模组加载器（Vanilla 表示没有）
    ModLoader::Kind m_pendingLoader = ModLoader::Kind::Vanilla;
    QString m_pendingGameVersion;
    QString m_pendingLoaderName;
    QString m_loaderVersion;
    QString m_loaderInstallerPath;

    QList<JavaInstall> m_java;
    QString m_stage;
    QString m_runningInstanceId;
    int m_memoryMb = 2048;
};
