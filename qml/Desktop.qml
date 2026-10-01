import QtQuick
import QtQuick.Window

// mcl 的根：一个【标准尺寸的窗口】，窗口内部自绘一整套 macOS 12 桌面。
//
// 层级（从下到上）：
//   1. 壁纸（清晰，C++ 侧预渲染）
//   2. 桌面交互层（空白处单击取消选中 / 右键菜单）
//   3. 窗口层    —— 自绘的应用窗口，可拖动 / 缩放 / 最小化 / 关闭
//   5. 菜单栏    —— 永远在最上
//   6. Dock      —— 永远在最上
Window {
    id: root

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 640
    visible: true
    title: qsTr("mcl")
    color: windowedMode ? "#0e1016" : "transparent"
    // 无边框是"复刻桌面"的前提；--windowed 时退回系统边框，方便调试与外接屏
    flags: windowedMode ? Qt.Window : (Qt.FramelessWindowHint | Qt.Window)

    readonly property int menuBarH: Theme.menuBarHeight
    readonly property int dockAreaH: Theme.dockHeight + Theme.dockBottomMargin
    // 工作区：菜单栏之下、Dock 之上。最大化窗口用它，避免压住菜单栏与 Dock
    readonly property real workX: 0
    readonly property real workY: menuBarH
    readonly property real workWidth: width
    readonly property real workHeight: Math.max(0, height - menuBarH - dockAreaH)

    // 最后一次在桌面按右键的位置 —— "新建实例"要在这个位置接着弹版本列表
    property real lastMenuX: 200
    property real lastMenuY: 200

    // 「新建实例」菜单的条目：上半截是纯原版，下半截是带 Fabric 的版本。
    // action 用 "vanilla:1.20.1" / "fabric:1.20.1" 这种前缀区分。
    readonly property var versionEntries: {
        const out = []
        const list = kernel.availableVersions
        const vanillaCount = Math.min(list.length, 5)
        for (let i = 0; i < vanillaCount; ++i)
            out.push({ text: list[i].name, action: "vanilla:" + list[i].id })

        // 加载器只列前几个版本，不然菜单会长得没法看
        const loaderCount = Math.min(list.length, 3)
        const loaders = [
            { id: "fabric", label: "Fabric" },
            { id: "forge", label: "Forge" },
            { id: "neoforge", label: "NeoForge" }
        ]
        for (let k = 0; k < loaders.length; ++k) {
            if (loaderCount === 0)
                break
            out.push({ text: "", action: "" })
            for (let i = 0; i < loaderCount; ++i)
                out.push({
                    text: list[i].name + "   + " + loaders[k].label,
                    action: loaders[k].id + ":" + list[i].id
                })
        }
        return out
    }

    // ——————————————————— 1. 壁纸
    Image {
        id: wallpaper
        anchors.fill: parent
        source: "image://mcl/wallpaper/" + shellSettings.appearanceKey
        sourceSize: Qt.size(1600, 1000)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        asynchronous: true
    }

    // ——————————————————— 2. 桌面交互层
    // 位于窗口层之下：有窗口的地方事件被窗口接管，空白处才落到这里
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                root.lastMenuX = mouse.x
                root.lastMenuY = mouse.y
                desktopMenu.popupAt(mouse.x, mouse.y)
            }
        }
    }

    // ——————————————————— 3. 窗口层
    Item {
        id: windowLayer
        anchors.fill: parent

        Repeater {
            model: desktop.windows
            delegate: McWindow {
                required property int windowId
                required property string appId
                required property string title
                required property real winX
                required property real winY
                required property real winWidth
                required property real winHeight
                required property bool minimized
                required property bool maximized
                // 注意：不能叫 z —— Item::z 在 Qt 6 里是 FINAL，覆盖不了
                required property int stackZ
                required property bool focused

                winId: windowId
                app: appId
                windowTitle: title
                posX: winX
                posY: winY
                width: winWidth
                height: winHeight
                isMinimized: minimized
                isMaximized: maximized
                isFocused: focused
                z: stackZ
                // 「关于本机」是苹果那种无标题栏的窄窗口
                chromeless: appId === "about"
                workArea: Qt.rect(root.workX, root.workY, root.workWidth, root.workHeight)
                minTop: root.menuBarH

                // model 是唯一的事实来源；拖动/缩放期间由窗口自己持有位置，松手后才写回
                onWinXChanged: if (!dragging) posX = winX
                onWinYChanged: if (!dragging) posY = winY
                onWinWidthChanged: if (!resizing) width = winWidth
                onWinHeightChanged: if (!resizing) height = winHeight
                onGeometryCommitted: function (nx, ny, nw, nh) {
                    desktop.setWindowGeometry(winId, nx, ny, nw, nh)
                }
            }
        }
    }

    // ——————————————————— 4. 菜单栏
    ShellMenuBar {
        id: menuBar
        width: parent.width
        height: root.menuBarH
        z: 1000
        desktopWidth: root.width
        desktopHeight: root.height
        onDragRequested: root.startSystemMove()
    }

    // ——————————————————— 5. Dock
    Dock {
        id: dock
        z: 1000
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.dockBottomMargin
        desktopWidth: root.width
        desktopHeight: root.height
    }

    // ——————————————————— 桌面右键菜单（macOS 的桌面菜单）
    McMenu {
        id: desktopMenu
        entries: [
            { text: qsTr("新建实例"), action: "newInstance" },
            { text: qsTr("下载整合包…"), action: "modpack", enabled: false },
            { text: "", action: "" },
            { text: qsTr("使用内置壁纸"), action: "wallpaperBuiltin" },
            { text: qsTr("跟随系统壁纸"), action: "wallpaperSystem" },
            { text: "", action: "" },
            { text: shellSettings.darkMode ? qsTr("切换到浅色外观") : qsTr("切换到深色外观"),
              action: "toggleDark" },
            { text: "", action: "" },
            { text: qsTr("显示视图选项…"), action: "options" }
        ]
        onTriggered: function (action) {
            if (action === "newInstance") {
                // 版本有九百多条，弹出菜单装不下 —— 交给专门的窗口
                desktop.openApp("newInstance")
                return
            }
            if (action === "wallpaperBuiltin")
                shellSettings.useBuiltinWallpaper()
            else if (action === "wallpaperSystem")
                shellSettings.useSystemWallpaper()
            else if (action === "toggleDark")
                shellSettings.darkMode = !shellSettings.darkMode
            else if (action === "options")
                desktop.openApp("settings")
        }
    }

    // ——————————————————— 「新建实例」选版本
    McMenu {
        id: newInstanceMenu
        entries: root.versionEntries.length > 0
                 ? root.versionEntries
                 : [{ text: qsTr("正在获取版本清单…"), action: "", enabled: false }]
        onTriggered: function (action) {
            if (action === "")
                return
            const parts = action.split(":")
            if (parts.length !== 2)
                return
            if (parts[0] === "fabric") {
                // Fabric 要先向 Meta 取加载器描述，建好了由 instanceCreated 翻出启动台
                kernel.createFabricInstance(parts[1], "")
                return
            }
            if (parts[0] === "forge" || parts[0] === "neoforge") {
                // Forge / NeoForge 要先装原版再跑 installer，同样是异步的
                kernel.createModdedInstance(parts[1], parts[0], "")
                return
            }
            if (kernel.createInstance(parts[1], "") !== "")
                desktop.overlay = "launchpad"
        }
    }

    // 从桌面右键新建实例时，建好了把启动台翻出来给用户看。
    // 注意**不能**在市场窗口里也翻 —— 那会把用户正在看的市场盖掉。
    Connections {
        target: kernel
        function onInstanceCreated(instanceId) {
            if (instanceId === "")
                return
            if (desktop.overlay === "launchpad")
                desktop.overlay = "" // 已经在启动台里，刷新一下即可
        }
    }

    // ——————————————————— 启动台（盖在桌面之上，但菜单栏与 Dock 仍在它上面）
    Launchpad {
        id: launchpad
        anchors.fill: parent
        opened: desktop.overlay === "launchpad"
        dockZoneHeight: root.dockAreaH + 34
        dockLeft: dock.x
        dockRight: dock.x + dock.width
        onRequestClose: desktop.overlay = ""
    }

    // 打开 mcl 就是为了启动游戏 —— 直接给出启动器窗口，不用用户先去 Dock 里找
    Component.onCompleted: {
        desktop.openApp("launcher")
        if (demoMode) {
            desktop.openApp("settings")
            desktop.openApp("about")
        }
    }
}
