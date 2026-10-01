import QtQuick
import QtQuick.Controls

// macOS 12 菜单栏。
//
// 毛玻璃是"真"的：整张模糊壁纸按桌面坐标铺在下面，这里只露出顶部 26px ——
// 所以它显示的就是它背后那块壁纸的模糊版本，和壁纸切换、窗口移动都自然对齐，
// 而且不需要 ShaderEffect / 合成器支持。
Item {
    id: bar

    property real desktopWidth: 1280
    property real desktopHeight: 800
    signal dragRequested()

    // 菜单项被点中后的语义分发
    function handleMenuAction(action) {
        switch (action) {
        case "about":      desktop.openApp("about"); break
        case "settings":   desktop.openApp("settings"); break
        case "launcher":   desktop.openApp("launcher"); break
        case "console":    desktop.openApp("console"); break
        case "toggleDark": shellSettings.darkMode = !shellSettings.darkMode; break
        case "signInMicrosoft": account.signInMicrosoft(); break
        case "signInOffline":   desktop.openApp("account"); break
        case "accountDetails":  desktop.openApp("account"); break
        case "signOut":         account.signOut(); break
        case "newInstance":     desktop.openApp("newInstance"); break
        case "quit":       Qt.quit(); break
        case "closeWindow": {
            const id = desktop.focusedWindowId();
            if (id > 0)
                desktop.closeWindow(id);
            break;
        }
        case "minimize": {
            const id = desktop.focusedWindowId();
            if (id > 0)
                desktop.toggleMinimize(id);
            break;
        }
        default: break;
        }
    }

    // ——————————————————— 毛玻璃底
    Item {
        anchors.fill: parent
        clip: true

        Image {
            // 只取顶部那一条：既避免整图缩放糊掉，也不用把整张底图搬进场景
            source: "image://mcl/glassrect/0/0/"
                    + Math.round(bar.desktopWidth) + "/" + Math.round(bar.height) + "/0/"
                    + Math.round(bar.desktopWidth) + "/" + Math.round(bar.desktopHeight) + "/"
                    + shellSettings.appearanceKey
            width: bar.desktopWidth
            height: bar.height
            x: 0
            y: 0
        }
        Rectangle {
            anchors.fill: parent
            // 固定深色 + 半透明：只盖一层很薄的色调，让下面那张模糊壁纸透出来。
            // 盖厚了就成了黑条，玻璃感全没了 —— 调大更实、调小更透。
            color: Theme.chromeTint
        }
        Image {
            anchors.fill: parent
            source: "image://mcl/noise/128"
            fillMode: Image.Tile
            opacity: 0.045
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.chromeSeparator
    }

    // 拖动菜单栏空白处移动整个 mcl 窗口
    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.LeftButton
        onPressed: bar.dragRequested()
    }

    // ——————————————————— 左侧菜单
    Row {
        id: menuRow
        anchors.left: parent.left
        anchors.leftMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        // 第一个位置：macOS 放苹果标。图已预处理成纯白 + 保留 alpha，
        // 所以它和菜单栏其他文字一样是浅色，切深浅模式都不用换资源。
        Item {
            width: 30
            height: bar.height
            Rectangle {
                anchors.centerIn: parent
                width: 26
                height: 20
                radius: 5
                color: brandMouse.containsMouse ? Theme.chromeHover : "transparent"
            }
            Image {
                anchors.centerIn: parent
                // 原图 209x256（略带竖向），按高度定尺寸，菜单栏里大约 13x16
                height: 16
                width: Math.round(height * 209 / 256)
                source: "qrc:/mcl/assets/apple-logo.png"
                sourceSize: Qt.size(width * 2, height * 2)
                fillMode: Image.PreserveAspectFit
                smooth: true
            }
            MouseArea {
                id: brandMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: aboutMenu.open()
            }

            McMenu {
                dark: true
                id: aboutMenu
                menuY: bar.height - 2
                menuX: 0
                // 苹果菜单：账户相关都放这儿
                entries: [
                    { text: account.menuLabel, action: "", enabled: false },
                    { text: "", action: "" },
                    { text: qsTr("登录微软账户…"), action: "signInMicrosoft",
                      enabled: !account.busy },
                    { text: qsTr("使用离线账户…"), action: "signInOffline" },
                    { text: qsTr("账户详情…"), action: "accountDetails" },
                    { text: qsTr("退出登录"), action: "signOut", enabled: account.signedIn },
                    { text: "", action: "" },
                    { text: qsTr("关于本机"), action: "about" },
                    { text: qsTr("系统设置…"), action: "settings" },
                    { text: "", action: "" },
                    { text: qsTr("退出 mcl"), action: "quit" }
                ]
                onTriggered: function (action) { bar.handleMenuAction(action) }
            }
        }

        Repeater {
            model: [
                { label: qsTr("文件"), entries: [
                      { text: qsTr("新建实例…"), action: "newInstance" },
                      { text: qsTr("打开数据目录"), action: "" },
                      { text: "", action: "" },
                      { text: qsTr("关闭窗口"), action: "closeWindow" }
                  ] },
                { label: qsTr("显示"), entries: [
                      { text: qsTr("控制台"), action: "console" },
                      { text: "", action: "" },
                      { text: qsTr("深色模式"), action: "toggleDark" }
                  ] },
                { label: qsTr("窗口"), entries: [
                      { text: qsTr("最小化"), action: "minimize" },
                      { text: qsTr("关闭"), action: "closeWindow" }
                  ] },
                { label: qsTr("帮助"), entries: [
                      { text: qsTr("mcl 项目说明"), action: "about" }
                  ] }
            ]

            delegate: Item {
                required property var modelData
                width: labelText.width + 20
                height: bar.height

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 2
                    height: bar.height - 6
                    radius: 5
                    color: (labelMouse.containsMouse || menu.opened) ? Theme.chromeHover : "transparent"
                }

                Text {
                    id: labelText
                    anchors.centerIn: parent
                    text: modelData.label
                    color: Theme.chromeText
                    font.pixelSize: 13
                }

                MouseArea {
                    id: labelMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: menu.opened ? menu.close() : menu.open()
                }

                McMenu {
                    dark: true
                    id: menu
                    menuY: bar.height - 2
                    menuX: 0
                    entries: modelData.entries
                    onTriggered: function (action) { bar.handleMenuAction(action) }
                }
            }
        }
    }

    // ——————————————————— 右侧状态区
    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        // 状态图标：Spotlight / 控制中心 / Wi-Fi / 电池（全部自绘）
        StatusIcons {
            anchors.verticalCenter: parent.verticalCenter
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: desktop.dateText + "  " + desktop.clockText
            color: Theme.chromeText
            font.pixelSize: 13
        }
    }
}
