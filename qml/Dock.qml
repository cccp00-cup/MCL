import QtQuick

// macOS 12 的 Dock：悬浮在桌面底部居中的一条毛玻璃，图标在其上放大、点击启动、
// 运行中的应用在图标下方显示指示点。
//
// 和菜单栏一样，毛玻璃是"真"的 —— 把整张模糊壁纸按桌面坐标摆好，这里只露出
// Dock 所在的那一段，于是它显示的就是它背后那块壁纸的模糊版本。
Item {
    id: dock

    property real desktopWidth: 1280
    property real desktopHeight: 800

    width: background.width
    height: Theme.dockHeight

    // —— 毛玻璃底
    Rectangle {
        id: background
        anchors.centerIn: parent
        width: iconRow.width + Theme.dockPadding * 2
        height: Theme.dockHeight
        radius: 20
        color: "transparent"
        border.width: 1
        border.color: Theme.chromeBorder
        clip: true

        Image {
            // 由 C++ 按 Dock 的真实位置裁一块、并切出圆角 —— QML 的 clip 只能裁矩形，
            // 圆角必须在这里做，否则四个角会被色调层重新填成直角。
            source: "image://mcl/glassrect/"
                    + Math.round(dock.x) + "/" + Math.round(dock.y) + "/"
                    + Math.round(dock.width) + "/" + Math.round(dock.height) + "/"
                    + background.radius + "/"
                    + Math.round(dock.desktopWidth) + "/" + Math.round(dock.desktopHeight) + "/"
                    + shellSettings.appearanceKey
            width: dock.width
            height: dock.height
            x: 0
            y: 0
        }
        Rectangle {
            anchors.fill: parent
            // 固定深色，和菜单栏同一套材质（比菜单栏再透一点）
            color: Theme.chromeTint
            // 必须跟 background 同样的圆角：QML 的 clip 只裁矩形，
            // 不加这句就会把上面那张已经裁好圆角的图重新填成直角。
            radius: background.radius
        }
        Image {
            anchors.fill: parent
            source: "image://mcl/noise/128"
            fillMode: Image.Tile
            opacity: 0.05
        }
    }

    // 鼠标位置 → 图标的放大计算
    MouseArea {
        id: hoverTracker
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        z: -1
    }

    Row {
        id: iconRow
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.dockPadding + 4
        spacing: 7

        Repeater {
            model: desktop.apps

            delegate: DockIcon {
                pointerX: hoverTracker.mouseX - iconRow.x
                pointerInside: hoverTracker.containsMouse
            }
        }

        // 应用区与右侧（分隔线 / 废纸篓）之间
        Rectangle {
            width: 1
            height: Theme.dockIconSize * 0.78
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.chromeSeparator
        }

        // 废纸篓（第一期是占位图标，点它没有动作）
        Item {
            width: Theme.dockIconSize
            height: Theme.dockIconSize

            Rectangle {
                anchors.fill: parent
                radius: Theme.dockIconRadius
                color: trashMouse.containsMouse ? Theme.chromeHoverStrong : Theme.chromeHover
                border.width: 1
                border.color: Theme.chromeBorder
            }

            Image {
                anchors.centerIn: parent
                width: 27
                height: 27
                source: "qrc:/mcl/icons/trash.svg"
                sourceSize: Qt.size(27, 27)
                smooth: true
            }

            MouseArea {
                id: trashMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
            }
        }
    }
}
