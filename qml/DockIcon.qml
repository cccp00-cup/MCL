import QtQuick

// Dock 上的一个应用图标。
//
// 两个 macOS 的招牌交互都在这里：
//   · 放大 —— 图标以底边为锚点放大，影响范围随鼠标距离衰减（一条曲线，不用 shader）
//   · 左键启动/切换，右键弹出应用菜单（显示所有窗口 / 退出）
Item {
    id: icon

    required property var modelData
    required property int index

    // 由 Dock 注入：鼠标在图标行坐标系里的位置
    property real pointerX: -9999
    property bool pointerInside: false

    readonly property bool running: desktop.isRunning(modelData.id)
    readonly property real centerX: x + width / 2

    width: Theme.dockIconSize
    height: Theme.dockIconSize

    readonly property real distance: Math.abs(centerX - pointerX)
    readonly property real influence: (pointerInside && distance < width * 1.9)
        ? (1 - distance / (width * 1.9)) : 0
    readonly property real magnify: 1 + 0.45 * Math.pow(influence, 1.45)

    Item {
        id: scaler
        anchors.fill: parent
        transformOrigin: Item.Bottom
        // 落在图标上时轻微下沉，松开弹回 —— macOS 的按压反馈
        scale: icon.magnify * (pressArea.pressed ? 0.94 : 1)
        // 轻微平滑：让放大/恢复不是硬切（时长压得很短，不至于跟手发浮）
        Behavior on scale {
            NumberAnimation { duration: 90; easing.type: Easing.OutQuad }
        }

        Rectangle {
            anchors.fill: parent
            radius: Theme.dockIconRadius
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.18)
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.lighter(modelData.accent, 1.30) }
                GradientStop { position: 1.0; color: Qt.darker(modelData.accent, 1.18) }
            }
        }

        // 玻璃质感：上缘受光、下缘压暗
        Rectangle {
            anchors.fill: parent
            radius: Theme.dockIconRadius
            gradient: Gradient {
                GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.32) }
                GradientStop { position: 0.42; color: Qt.rgba(1, 1, 1, 0.04) }
                GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.12) }
            }
        }

        Image {
            anchors.centerIn: parent
            width: 27
            height: 27
            source: modelData.icon
            sourceSize: Qt.size(27, 27)
            smooth: true
        }
    }

    // 运行指示点
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.bottom
        anchors.topMargin: 3
        width: 4
        height: 4
        radius: 2
        color: Theme.chromeTextDim
        visible: icon.running
    }

    // 悬浮显示应用名（macOS 的 tooltip）
    Rectangle {
        id: tip
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.top
        anchors.bottomMargin: 10
        width: tipText.width + 16
        height: 22
        radius: 6
        color: Theme.chromeMenuFill
        border.width: 1
        border.color: Theme.chromeMenuBorder
        opacity: pressArea.containsMouse ? 1 : 0
        visible: opacity > 0.01
        Behavior on opacity {
            NumberAnimation { duration: Theme.durationFast }
        }

        Text {
            id: tipText
            anchors.centerIn: parent
            text: modelData.name
            font.pixelSize: 12
            color: Theme.chromeText
        }
    }

    // —— 右键菜单（与菜单栏下拉共用 McMenu）
    McMenu {
        id: iconMenu
        dark: true
        entries: modelData.kind === "instance"
            ? [
                  { text: qsTr("启动"), action: "launch" },
                  { text: "", action: "" },
                  { text: qsTr("从 Dock 移除"), action: "unpin" }
              ]
            : [
                  { text: qsTr("显示所有窗口"), action: "show" },
                  { text: qsTr("新建窗口"), action: "show" },
                  { text: "", action: "" },
                  { text: qsTr("退出 %1").arg(modelData.name), action: "quit" }
              ]
        onTriggered: function (action) {
            if (action === "launch")
                kernel.launchOffline(modelData.id, "Player")
            else if (action === "unpin")
                kernel.pinInstance(modelData.id, false)
            else if (action === "quit")
                desktop.quitApp(modelData.id)
            else if (action === "show")
                desktop.openApp(modelData.id)
        }
    }

    MouseArea {
        id: pressArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.PointingHandCursor
        onClicked: function (mouse) {
            if (mouse.button === Qt.RightButton) {
                iconMenu.popupAt(mouse.x - 42, mouse.y - 6, true)
                return
            }
            // 实例是"启动游戏"，应用是"开窗口"
            if (modelData.kind === "instance")
                kernel.launchOffline(modelData.id, "Player")
            else
                desktop.openApp(modelData.id)
        }
    }
}
