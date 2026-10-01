import QtQuick

// 自绘的 macOS 12 风格窗口。
//
// 它不是操作系统的真窗口，只是桌面里的一块矩形，因此拖动、缩放、层级全部由
// WindowModel 记录 —— 这样"下次打开恢复上次布局"只是把 model 存下来而已。
//
// 与 macOS 一致的交互：
//   · 拖动标题栏移动窗口，拖动时不允许越过菜单栏
//   · 双击标题栏 = 缩放（最大化到工作区）
//   · 红灯关闭窗口（应用继续留在 Dock 上）、黄灯最小化、绿灯最大化
//   · 右下角 / 右边缘 / 下边缘可缩放
Item {
    id: win

    // ——— 由 Desktop.qml 注入 ———
    property int winId: 0
    property string app: ""
    property string windowTitle: ""
    property real posX: 0
    property real posY: 0
    property bool isMinimized: false
    property bool isMaximized: false
    property bool isFocused: true
    property rect workArea: Qt.rect(0, 0, 1280, 800)
    property real minTop: 0
    // 去掉标题栏的窗口（苹果那种「关于本机」）。内容铺满整个窗口，
    // 关闭按钮由内容自己提供。
    property bool chromeless: false

    // ——— 状态 ———
    readonly property bool dragging: titleDrag.active
    readonly property bool resizing: resizeCorner.active || resizeRight.active
                                      || resizeBottom.active
    signal geometryCommitted(real x, real y, real w, real h)

    // 各应用的内容组件。写成函数而不是内联 switch，是因为 Loader.sourceComponent
    // 那行得用三元表达式区分浏览器（它必须走字符串 source），而 QML 的 JS
    // 不支持在三元里直接放 {} block。
    function componentFor(appId) {
        switch (appId) {
        case "launcher":  return compLauncher
        case "mods":      return compModMarket
        case "packs":     return compPackMarket
        case "resources": return compResourceMarket
        case "shaders":   return compShaderMarket
        case "instanceSettings": return compInstanceSettings
        case "console":   return compConsole
        case "settings":  return compSettings
        case "about":     return compAbout
        case "account":   return compAccount
        case "newInstance": return compNewInstance
        case "intro":     return compIntro
        default:          return compPlaceholder
        }
    }

    x: posX
    y: posY
    opacity: isMinimized ? 0 : reveal
    visible: opacity > 0.01
    z: 1

    property real reveal: 0
    Behavior on opacity {
        NumberAnimation { duration: Theme.durationNormal; easing.type: Easing.OutCubic }
    }
    NumberAnimation on reveal {
        from: 0
        to: 1
        duration: 170
        easing.type: Easing.OutCubic
    }

    // ——————————————————— 阴影：多层描边环近似柔和投影（不用 ShaderEffect）
    // 层数越多、单层越淡，扩散就越接近高斯；并且向下扩散更多 —— 光是从上方来的
    Repeater {
        model: 7
        delegate: Rectangle {
            required property int index
            anchors.fill: parent
            anchors.leftMargin: -(index + 1) * 3.0
            anchors.rightMargin: -(index + 1) * 3.0
            anchors.topMargin: -(index + 1) * 1.8
            anchors.bottomMargin: -(index + 1) * 4.4
            radius: Theme.windowRadius + (index + 1) * 3.2
            color: "transparent"
            border.width: 3.6
            border.color: Qt.rgba(0, 0, 0, (win.isFocused ? 0.11 : 0.07) * (1.0 - index / 7.0))
            z: -2
        }
    }

    // ——————————————————— 窗口本体
    Rectangle {
        id: chrome
        anchors.fill: parent
        radius: Theme.windowRadius
        // 无标题栏的窗口自己画底（磨砂玻璃），这里就不铺了
        color: win.chromeless ? "transparent" : Theme.windowBody
        border.width: win.chromeless ? 0 : 1
        border.color: Theme.windowBorder
        clip: true

        // —— 标题栏
        Rectangle {
            id: titleBar
            visible: !win.chromeless
            width: parent.width
            height: win.chromeless ? 0 : Theme.titleBarHeight
            radius: Theme.windowRadius
            color: win.isFocused ? Theme.windowTitleBar : Qt.lighter(Theme.windowTitleBar, 1.05)

            // 抹平下方两个圆角：标题栏只保留上圆角
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: Theme.windowRadius
                color: titleBar.color
            }

            // —— 交通灯
            Row {
                id: traffic
                x: 13
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Repeater {
                    model: [
                        { dot: Theme.trafficClose, glyph: "×", act: "close" },
                        { dot: Theme.trafficMinimize, glyph: "−", act: "min" },
                        { dot: Theme.trafficZoom, glyph: "+", act: "zoom" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        width: 12
                        height: 12
                        radius: 6
                        color: modelData.dot
                        border.width: 0.5
                        border.color: Qt.rgba(0, 0, 0, 0.12)

                        Text {
                            anchors.centerIn: parent
                            text: modelData.glyph
                            font.pixelSize: 9
                            font.bold: true
                            color: Qt.rgba(0, 0, 0, 0.55)
                            visible: trafficMouse.containsMouse
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.ArrowCursor
                            onClicked: {
                                if (modelData.act === "close")
                                    desktop.closeWindow(win.winId)
                                else if (modelData.act === "min")
                                    desktop.toggleMinimize(win.winId)
                                else
                                    desktop.toggleMaximize(win.winId, workArea.x, workArea.y,
                                                           workArea.width, workArea.height)
                            }
                        }
                    }
                }
            }

            // 交通灯的悬浮探测：必须放在 Row 外面 —— 放进 Row 会参与它的布局，
            // 而它的宽度又依赖 Row 的宽度，直接形成 polish 死循环。
            MouseArea {
                id: trafficMouse
                x: 7
                y: (titleBar.height - 24) / 2
                width: traffic.width + 12
                height: 24
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }

            // —— 标题
            Text {
                anchors.centerIn: parent
                width: parent.width - 160
                text: win.windowTitle
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: win.isFocused ? Theme.textPrimary : Theme.textTertiary
            }

            // —— 拖动 / 双击缩放
            MouseArea {
                id: titleDrag
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.leftMargin: 84          // 让开交通灯
                acceptedButtons: Qt.LeftButton | Qt.RightButton

                property bool active: false
                property real startMouseX: 0
                property real startMouseY: 0
                property real startWinX: 0
                property real startWinY: 0

                onPressed: function (mouse) {
                    desktop.focusWindow(win.winId)
                    if (mouse.button !== Qt.LeftButton)
                        return
                    active = true
                    startWinX = win.posX
                    startWinY = win.posY
                    const p = mapToItem(win.parent, mouse.x, mouse.y)
                    startMouseX = p.x
                    startMouseY = p.y
                }
                onPositionChanged: function (mouse) {
                    if (!active)
                        return
                    const p = mapToItem(win.parent, mouse.x, mouse.y)
                    win.posX = startWinX + (p.x - startMouseX)
                    // macOS 的窗口越不过菜单栏
                    win.posY = Math.max(win.minTop, startWinY + (p.y - startMouseY))
                }
                onReleased: {
                    if (!active)
                        return
                    active = false
                    win.geometryCommitted(win.posX, win.posY, win.width, win.height)
                }
                onDoubleClicked: desktop.toggleMaximize(win.winId, workArea.x, workArea.y,
                                                        workArea.width, workArea.height)
            }
        }

        // —— 无标题栏时的拖动
        // 没有标题栏可抓，就让整块背景都能拖。z:-1 压在内容之下 ——
        // Rectangle / Image 本身不接收鼠标，点击会透到这儿来；
        // 内容里的按钮有自己的 MouseArea，不受影响。
        MouseArea {
            id: chromeDrag
            anchors.fill: parent
            visible: win.chromeless
            z: -1
            acceptedButtons: Qt.LeftButton

            property real startMouseX: 0
            property real startMouseY: 0
            property real startWinX: 0
            property real startWinY: 0

            onPressed: function (mouse) {
                desktop.focusWindow(win.winId)
                startWinX = win.posX
                startWinY = win.posY
                const p = mapToItem(win.parent, mouse.x, mouse.y)
                startMouseX = p.x
                startMouseY = p.y
            }
            onPositionChanged: function (mouse) {
                if (!pressed)
                    return
                const p = mapToItem(win.parent, mouse.x, mouse.y)
                win.posX = startWinX + (p.x - startMouseX)
                win.posY = Math.max(win.minTop, startWinY + (p.y - startMouseY))
            }
            onReleased: win.geometryCommitted(win.posX, win.posY, win.width, win.height)
        }

        // —— 标题栏分隔线
        Rectangle {
            id: titleSeparator
            anchors.top: titleBar.bottom
            width: parent.width
            height: win.chromeless ? 0 : 1
            color: Theme.separator
        }

        // —— 内容
        //
        // 浏览器**单独走字符串 source**：AppBrowser.qml 里 import 了 QtWebEngine，
        // 而没有那个模块的环境（没装 qt6-webengine-dev）连 `Component { AppBrowser {} }`
        // 这种声明都会在编译期报 "module QtWebEngine is not installed"。
        // 用字符串就只在实际打开浏览器时才去加载它。
        Loader {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: win.chromeless ? parent.top : titleSeparator.bottom
            anchors.bottom: parent.bottom

            source: (win.app === "browser" && hasBrowser) ? browserUrl : ""
            sourceComponent: (win.app === "browser") ? null : componentFor(win.app)

            // 浏览器走 RESOURCES 那条 qrc 路径，不是 qrc:/qt/qml —— 见 CMakeLists
            readonly property string browserUrl: "qrc:/mcl/qml/AppBrowser.qml"
        }

        // —— 缩放热区
        MouseArea {
            id: resizeCorner
            width: 18
            height: 18
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor

            property bool active: false
            property real startMouseX: 0
            property real startMouseY: 0
            property real startW: 0
            property real startH: 0

            onPressed: function (mouse) {
                desktop.focusWindow(win.winId)
                active = true
                startW = win.width
                startH = win.height
                const p = mapToItem(win.parent, mouse.x, mouse.y)
                startMouseX = p.x
                startMouseY = p.y
            }
            onPositionChanged: function (mouse) {
                if (!active)
                    return
                const p = mapToItem(win.parent, mouse.x, mouse.y)
                win.width = Math.max(360, startW + (p.x - startMouseX))
                win.height = Math.max(240, startH + (p.y - startMouseY))
            }
            onReleased: {
                if (!active)
                    return
                active = false
                win.geometryCommitted(win.posX, win.posY, win.width, win.height)
            }
        }

        MouseArea {
            id: resizeRight
            width: 5
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            cursorShape: Qt.SizeHorCursor

            property bool active: false
            property real startMouseX: 0
            property real startW: 0

            onPressed: function (mouse) {
                desktop.focusWindow(win.winId)
                active = true
                startW = win.width
                startMouseX = mapToItem(win.parent, mouse.x, mouse.y).x
            }
            onPositionChanged: function (mouse) {
                if (!active)
                    return
                win.width = Math.max(360, startW + (mapToItem(win.parent, mouse.x, mouse.y).x
                                                     - startMouseX))
            }
            onReleased: {
                if (!active)
                    return
                active = false
                win.geometryCommitted(win.posX, win.posY, win.width, win.height)
            }
        }

        MouseArea {
            id: resizeBottom
            height: 5
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeVerCursor

            property bool active: false
            property real startMouseY: 0
            property real startH: 0

            onPressed: function (mouse) {
                desktop.focusWindow(win.winId)
                active = true
                startH = win.height
                startMouseY = mapToItem(win.parent, mouse.x, mouse.y).y
            }
            onPositionChanged: function (mouse) {
                if (!active)
                    return
                win.height = Math.max(240, startH + (mapToItem(win.parent, mouse.x, mouse.y).y
                                                      - startMouseY))
            }
            onReleased: {
                if (!active)
                    return
                active = false
                win.geometryCommitted(win.posX, win.posY, win.width, win.height)
            }
        }
    }

    // 点窗口任意处即聚焦
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        z: -1
        onClicked: desktop.focusWindow(win.winId)
    }

    // ——————————————————— 各应用的内容
    Component { id: compLauncher; AppLauncher {} }
    Component { id: compModMarket; AppModMarket {} }
    Component { id: compPackMarket; AppPackMarket {} }
    Component { id: compResourceMarket; AppResourceMarket {} }
    Component { id: compShaderMarket; AppShaderMarket {} }
    Component { id: compInstanceSettings; AppInstanceSettings {} }
    Component { id: compConsole; AppConsole {} }
    Component { id: compSettings; AppSettings {} }
    Component { id: compAbout; AppAbout {} }
    Component { id: compAccount; AppAccount {} }
    Component { id: compNewInstance; AppNewInstance {} }
    Component { id: compIntro; AppIntro {} }

    Component {
        id: compPlaceholder
        Item {
            Text {
                anchors.centerIn: parent
                text: qsTr("（此应用尚未实现）")
                color: Theme.textTertiary
                font.pixelSize: 13
            }
        }
    }
}
