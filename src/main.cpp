#include <cstdio>
#include <QFont>
#include <QClipboard>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include "kernel/AccountManager.h"
#include "kernel/InstanceStore.h"
#include "kernel/Mirror.h"
#include "kernel/ModrinthClient.h"
#include "kernel/VanillaKernel.h"
#include "Log.h"
#include "shell/DesktopController.h"
#include "shell/GlassProvider.h"
#include "shell/ShellSettings.h"

namespace
{
// 从命令行里取 "--flag value" / "--flag=value"
QString optionValue(const QStringList &args, const QString &name)
{
    const QString withEq = name + QLatin1Char('=');
    for (int i = 0; i < args.size(); ++i) {
        if (args.at(i) == name && i + 1 < args.size())
            return args.at(i + 1);
        if (args.at(i).startsWith(withEq))
            return args.at(i).mid(withEq.size());
    }
    return {};
}
} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // Windows 上没有控制台，日志得落文件；其它平台这里什么都不做
    Log::install();

    // 任务栏 / 标题栏图标。Linux 下由 .desktop 负责，这里主要是给 Windows 用。
    app.setWindowIcon(QIcon(QStringLiteral("qrc:/mcl/icons/png/mcl-256.png")));
    app.setApplicationName(QStringLiteral("mcl"));
    app.setApplicationDisplayName(QStringLiteral("mcl"));
    // 不设 organizationName：否则 QStandardPaths::AppConfigLocation 会变成
    // ~/.config/mcl/mcl（org/app 各一层），配置目录凭空多一层。
    app.setApplicationVersion(QStringLiteral("0.3.1"));
    app.setDesktopFileName(QStringLiteral("mcl"));

    // 界面全部自绘，不依赖任何平台控件样式
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // 字体回退链：macOS 上直接命中真 SF Pro；Linux 上依次退到 Inter / Cantarell / Noto。
    // 不内置字体（版权 + 体积），但在任何平台上都能拿到最接近系统 UI 的那一款。
    {
        QFont uiFont;
        uiFont.setFamilies({ QStringLiteral("SF Pro Text"), QStringLiteral("SF Pro Display"),
                             QStringLiteral(".AppleSystemUIFont"), QStringLiteral("Inter"),
                             QStringLiteral("Cantarell"), QStringLiteral("Noto Sans"),
                             QStringLiteral("Noto Sans CJK SC"), QStringLiteral("sans-serif") });
        uiFont.setPixelSize(13);
        uiFont.setHintingPreference(QFont::PreferFullHinting);
        QGuiApplication::setFont(uiFont);
    }

    const QStringList args = app.arguments();

    // 默认走 BMCLAPI 镜像；MCL_MIRROR=official 可以切回官方源
    if (qEnvironmentVariable("MCL_MIRROR") == QLatin1String("official"))
        Mirror::setSource(Mirror::Source::Official);

    // mcl 是「桌面外壳 + 内嵌内核」两层。内核是自研的 Vanilla 实现：
    // 自己读官方版本清单、下载库与资源、解压 natives、拼 java 命令行、起进程 ——
    // 不依赖 PrismLauncher，也不把启动转交给别的程序。
    // 两个市场各用一个客户端，免得同时打开时互相覆盖检索结果
    ModrinthClient modMarket;
    ModrinthClient packMarket;
    ModrinthClient resourceMarket;
    ModrinthClient shaderMarket;

    AccountManager account;
    InstanceStore instanceStore;
    VanillaKernel kernel(&instanceStore, &account);

    ShellSettings settings;
    // --dark 以深色外观启动（也方便对比两种模式）
    if (args.contains(QStringLiteral("--dark")))
        settings.overrideDarkMode(true);
    DesktopController desktop(&instanceStore);

    QQmlApplicationEngine engine;
    // 壁纸 / 毛玻璃底材。engine 接管所有权。
    engine.addImageProvider(QStringLiteral("mcl"), new GlassProvider(&settings));

    engine.rootContext()->setContextProperty(QStringLiteral("shellSettings"), &settings);
    engine.rootContext()->setContextProperty(QStringLiteral("desktop"), &desktop);
    engine.rootContext()->setContextProperty(QStringLiteral("kernel"), &kernel);
    engine.rootContext()->setContextProperty(QStringLiteral("instanceStore"), &instanceStore);
    engine.rootContext()->setContextProperty(QStringLiteral("account"), &account);
    engine.rootContext()->setContextProperty(QStringLiteral("modMarket"), &modMarket);
    engine.rootContext()->setContextProperty(QStringLiteral("packMarket"), &packMarket);
    engine.rootContext()->setContextProperty(QStringLiteral("resourceMarket"), &resourceMarket);
    engine.rootContext()->setContextProperty(QStringLiteral("shaderMarket"), &shaderMarket);
    engine.rootContext()->setContextProperty(QStringLiteral("windowedMode"),
                                             args.contains(QStringLiteral("--windowed")));
    // 演示模式：一次打开多个窗口，便于截图与观察窗口层级
    engine.rootContext()->setContextProperty(QStringLiteral("demoMode"),
                                             args.contains(QStringLiteral("--demo")));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     []() { QCoreApplication::exit(2); }, Qt::QueuedConnection);

    engine.loadFromModule("Mcl", "Desktop");
    if (engine.rootObjects().isEmpty())
        return 1;

    // 调试：直接走一遍微软登录，验证链路
    if (args.contains(QStringLiteral("--ms-login"))) {
        QObject::connect(&account, &AccountManager::deviceCodeReady, &app,
                         [](const QString &code, const QString &uri) {
                             Log::line(QString());
                             Log::line(QStringLiteral("[mcl] ===== 设备码: %1 =====").arg(code));
                             Log::line(QStringLiteral("[mcl] 请在浏览器打开: %1").arg(uri));

                             // 顺手确认设备码有没有真的进剪贴板
                             QClipboard *clipboard = QGuiApplication::clipboard();
                             const QString clip = clipboard ? clipboard->text()
                                                            : QStringLiteral("<无剪贴板>");
                             Log::line(QStringLiteral("[mcl] 剪贴板=%1  复制%2")
                                           .arg(clip, clipboard && clip.size() == 8
                                                          ? QStringLiteral("成功")
                                                          : QStringLiteral("失败")));
                             Log::line(QString());
                         });
        QObject::connect(&account, &AccountManager::failed, &app,
                         [](const QString &message) {
                             Log::line(QStringLiteral("[mcl] 登录失败: %1").arg(message));
                         });
        QObject::connect(&account, &AccountManager::changed, &app, [&account]() {
            Log::line(QStringLiteral("[mcl] 状态: %1").arg(account.status()));
        });
        QTimer::singleShot(400, &account, &AccountManager::signInMicrosoft);
        QTimer::singleShot(12000, &app, &QCoreApplication::quit);
    }

    // 内核日志交给日志出口：Linux 上直接进终端，Windows 上进 mcl.log
    QObject::connect(&kernel, &McKernel::logLine, &app, [](const QString &line) {
        Log::line(line);
    });

    // --launch <版本号>：跳过鼠标操作，直接把某个版本跑一遍（调试/验证用）
    const QString launchId = optionValue(args, QStringLiteral("--launch"));
    if (!launchId.isEmpty()) {
        QTimer::singleShot(300, &kernel,
                           [&kernel, launchId]() { kernel.launchOffline(launchId, QStringLiteral("Dev")); });
    }

    // --new-instance <版本号>：建一个实例（调试用）
    const QString newInstanceVersion = optionValue(args, QStringLiteral("--new-instance"));
    if (!newInstanceVersion.isEmpty())
        kernel.createInstance(newInstanceVersion, QString());

    // --pack <Modrinth 项目 id>：直接装一个整合包（调试用）
    const QString packId = optionValue(args, QStringLiteral("--pack"));
    if (!packId.isEmpty()) {
        QTimer::singleShot(400, &kernel,
                           [&kernel, packId]() { kernel.installModpack(packId, QString()); });
    }

    // --settings <实例 id>：直接打开某个实例的设置窗口（调试用）
    const QString settingsId = optionValue(args, QStringLiteral("--settings"));
    if (!settingsId.isEmpty()) {
        QTimer::singleShot(500, &desktop, [&desktop, settingsId]() {
            desktop.setEditingInstance(settingsId);
            desktop.openApp(QStringLiteral("instanceSettings"));
        });
    }

    // --mods <实例 id>：按某个实例打开模组市场（调试用）。
    // 走的是和「实例设置 → 添加模组…」同一条路：先绑实例再开窗口，
    // 所以市场会照该实例的 MC 版本预填筛选。
    const QString modsId = optionValue(args, QStringLiteral("--mods"));
    if (!modsId.isEmpty()) {
        QTimer::singleShot(500, &desktop, [&desktop, &modMarket, modsId]() {
            modMarket.setTargetInstance(modsId);
            desktop.openApp(QStringLiteral("mods"));
        });
    }

    // --open <应用 id>：启动后直接打开某个应用窗口（调试/截图用）
    const QString openApp = optionValue(args, QStringLiteral("--open"));
    if (!openApp.isEmpty()) {
        QTimer::singleShot(500, &desktop,
                           [&desktop, openApp]() { desktop.openApp(openApp); });
    }

    // --launchpad：启动后直接铺开启动台（调试/截图用）
    if (args.contains(QStringLiteral("--launchpad")))
        desktop.setOverlay(QStringLiteral("launchpad"));

    // --install <版本号>：只装不跑，装完就退出（用来量下载/校验的耗时）
    const QString installId = optionValue(args, QStringLiteral("--install"));
    if (!installId.isEmpty()) {
        QObject::connect(&kernel, &McKernel::launchFinished, &app,
                         [](const QString &, int) { QCoreApplication::quit(); });
        QTimer::singleShot(300, &kernel,
                           [&kernel, installId]() { kernel.install(installId); });
    }

    // --forge <原版版本>：建一个 Forge 实例并跑起来（调试用，NeoForge 见 --neoforge）
    const QString forgeVersion = optionValue(args, QStringLiteral("--forge"));
    if (!forgeVersion.isEmpty()) {
        QObject::connect(&kernel, &VanillaKernel::instanceCreated, &app,
                         [&kernel](const QString &instanceId) {
                             if (!instanceId.isEmpty())
                                 kernel.launchOffline(instanceId, QStringLiteral("Dev"));
                         });
        QTimer::singleShot(400, &kernel, [&kernel, forgeVersion]() {
            kernel.createModdedInstance(forgeVersion, QStringLiteral("forge"), QString());
        });
    }

    // --neoforge <原版版本>：同上，走 NeoForge
    const QString neoForgeVersion = optionValue(args, QStringLiteral("--neoforge"));
    if (!neoForgeVersion.isEmpty()) {
        QObject::connect(&kernel, &VanillaKernel::instanceCreated, &app,
                         [&kernel](const QString &instanceId) {
                             if (!instanceId.isEmpty())
                                 kernel.launchOffline(instanceId, QStringLiteral("Dev"));
                         });
        QTimer::singleShot(400, &kernel, [&kernel, neoForgeVersion]() {
            kernel.createModdedInstance(neoForgeVersion, QStringLiteral("neoforge"), QString());
        });
    }

    // --launch-fabric <原版版本>：建一个 Fabric 实例并直接跑起来（调试用）
    const QString launchFabric = optionValue(args, QStringLiteral("--launch-fabric"));
    if (!launchFabric.isEmpty()) {
        QObject::connect(&kernel, &VanillaKernel::instanceCreated, &app,
                         [&kernel](const QString &instanceId) {
                             if (!instanceId.isEmpty())
                                 kernel.launchOffline(instanceId, QStringLiteral("Dev"));
                         });
        QTimer::singleShot(400, &kernel, [&kernel, launchFabric]() {
            kernel.createFabricInstance(launchFabric, QString());
        });
    }

    // 冒烟测试用：渲染若干帧后抓一张图存盘再退出
    const QString shotPath = optionValue(args, QStringLiteral("--shot"));
    if (!shotPath.isEmpty()) {
        QTimer::singleShot(2600, &app, [&engine, shotPath]() {
            if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
                const QImage image = window->grabWindow();
                if (!image.isNull())
                    image.save(shotPath);
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
