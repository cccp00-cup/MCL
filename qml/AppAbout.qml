import QtQuick

// 「关于本机」—— 苹果那种竖向窄卡片，没有标题栏。
//
// 玻璃不是画出来的假象：按**窗口在桌面上的实际位置**从壁纸的模糊版本里取对应
// 那一块（GlassProvider 的 glassrect 端点），所以它会跟着背后的壁纸变。
//
// 这里必须带 `win.` 前缀访问属性：QML 动态创建的组件能继承创建点的 **id**，
// 但看不到那边的属性 —— 直接写 desktopWidth 会 ReferenceError。
Item {
    id: root

    // ——————————————— 磨砂玻璃底
    Image {
        id: glass
        anchors.fill: parent
        // x / y / w / h / 圆角 / 桌面宽高
        source: "image://mcl/glassrect/" + win.posX + "/" + win.posY + "/"
                + width + "/" + height + "/" + Theme.windowRadius + "/"
                + win.workArea.width + "/" + win.workArea.height
        sourceSize: Qt.size(Math.max(1, Math.ceil(width)), Math.max(1, Math.ceil(height)))
        visible: status === Image.Ready
    }

    // 兜底底材：玻璃还没出来（或者取不到）时垫着，圆角要和窗口一致
    Rectangle {
        anchors.fill: parent
        radius: Theme.windowRadius
        color: shellSettings.darkMode ? Qt.rgba(0.12, 0.12, 0.14, 0.92)
                                      : Qt.rgba(0.97, 0.97, 0.98, 0.92)
        z: -1
    }

    // 玻璃之上压一层很淡的底 —— 不然后面花花绿绿的壁纸会把文字冲掉
    Rectangle {
        anchors.fill: parent
        radius: Theme.windowRadius
        color: shellSettings.darkMode ? Qt.rgba(0.06, 0.06, 0.08, 0.32)
                                      : Qt.rgba(1, 1, 1, 0.36)
    }

    // ——————————————— 关闭按钮（右上角，替代交通灯）
    Rectangle {
        id: closeButton
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 11
        anchors.topMargin: 11
        width: 22
        height: 22
        radius: 11
        color: closeMouse.containsMouse
               ? (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.20) : Qt.rgba(0, 0, 0, 0.10))
               : (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(0, 0, 0, 0.05))

        // 一个 ×，用两根细矩形拼，省得依赖图标字体
        Item {
            anchors.centerIn: parent
            width: 11
            height: 11
            Rectangle {
                anchors.centerIn: parent
                width: 11
                height: 1.4
                radius: 0.7
                color: Theme.textSecondary
                rotation: 45
            }
            Rectangle {
                anchors.centerIn: parent
                width: 11
                height: 1.4
                radius: 0.7
                color: Theme.textSecondary
                rotation: -45
            }
        }

        MouseArea {
            id: closeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: desktop.closeWindow(win.winId)
        }
    }

    // ——————————————— 图标 + 名称 / 版本（居中）
    Column {
        id: header
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 42
        spacing: 9

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 92
            height: 92
            radius: 20
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#6fb2ff" }
                GradientStop { position: 1.0; color: "#13408e" }
            }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.28)
            }
            Image {
                anchors.centerIn: parent
                width: 56
                height: 56
                source: "qrc:/mcl/icons/cube.svg"
                sourceSize: Qt.size(56, 56)
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "mcl"
            font.pixelSize: 21
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("版本 %1").arg(Qt.application.version)
            font.pixelSize: 11
            color: Theme.textSecondary
        }
    }

    // ——————————————— 分隔线
    Rectangle {
        id: divider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.leftMargin: 26
        anchors.rightMargin: 26
        anchors.topMargin: 18
        height: 1
        color: Theme.separator
    }

    // ——————————————— 信息（窄窗口用"标签在上、值在下"的堆叠）
    Column {
        id: info
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: divider.bottom
        anchors.leftMargin: 26
        anchors.rightMargin: 26
        anchors.topMargin: 14
        spacing: 11

        Repeater {
            model: [
                { k: qsTr("启动内核"), v: qsTr("mcl 自研内核") },
                { k: qsTr("平台"), v: Qt.platform.os },
                { k: qsTr("界面"), v: qsTr("Qt 6 · QML 全自绘") }
            ]

            delegate: Column {
                required property var modelData
                width: info.width
                spacing: 1

                Text {
                    text: modelData.k
                    font.pixelSize: 10
                    color: Theme.textTertiary
                }
                Text {
                    width: parent.width
                    text: modelData.v
                    font.pixelSize: 12
                    color: Theme.textPrimary
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    // ——————————————— 底部按钮
    Rectangle {
        id: detailsButton
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        width: detailsLabel.width + 30
        height: 27
        radius: 7
        color: detailsMouse.containsMouse
               ? (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(0, 0, 0, 0.08))
               : (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.10) : Qt.rgba(1, 1, 1, 0.60))
        border.width: 1
        border.color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(0, 0, 0, 0.10)

        Text {
            id: detailsLabel
            anchors.centerIn: parent
            text: qsTr("项目说明")
            font.pixelSize: 11
            color: Theme.textPrimary
        }
        MouseArea {
            id: detailsMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            // 苹果的「更多信息…」在这儿：直接跳到项目说明
            onClicked: desktop.openApp("intro")
        }
    }
}
