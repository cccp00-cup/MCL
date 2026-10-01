import QtQuick

// 「关于本机」应用 —— 苹果那种长方形磨砂玻璃对话框。
//
// 玻璃不是画出来的假象：按**窗口在桌面上的实际位置**从壁纸的模糊版本里取对应
// 那一块（GlassProvider 的 glassrect 端点），所以它会跟着背后的壁纸变 ——
// 壁纸是紫的，这块就是紫的；壁纸换了，它也跟着换。
//
// 坐标用的是 win.posX / win.posY（窗口在桌面里的位置），和 GlassProvider
// 那套桌面坐标是同一套。
//
// 注意这里必须带 `win.` 前缀：QML 动态创建的组件能继承创建点的 **id**，
// 但**看不到那边的属性** —— 直接写 desktopWidth 会 ReferenceError。
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
        // 图还没出来时别留个洞
        visible: status === Image.Ready
    }

    // 兜底底材。玻璃出来之前/之后都垫着，只是玻璃一盖上去就看不见了。
    Rectangle {
        anchors.fill: parent
        color: shellSettings.darkMode ? Qt.rgba(0.10, 0.10, 0.12, 0.72)
                                      : Qt.rgba(0.98, 0.98, 0.99, 0.72)
        z: -1
    }

    // 玻璃之上再压一层很淡的底 —— 不然密密麻麻的图标/文字压上去会读不清
    Rectangle {
        anchors.fill: parent
        color: shellSettings.darkMode ? Qt.rgba(0.06, 0.06, 0.08, 0.30)
                                      : Qt.rgba(1, 1, 1, 0.34)
    }

    // ——————————————— 图标 + 名称 / 版本
    Row {
        id: header
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 28
        anchors.topMargin: 30
        spacing: 18

        Rectangle {
            width: 82
            height: 82
            radius: 18
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#6fb2ff" }
                GradientStop { position: 1.0; color: "#13408e" }
            }
            // 图标自身的一点点内阴影感
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                color: "transparent"
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.28)
            }

            Image {
                anchors.centerIn: parent
                width: 50
                height: 50
                source: "qrc:/mcl/icons/cube.svg"
                sourceSize: Qt.size(50, 50)
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Text {
                text: "mcl"
                font.pixelSize: 25
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }
            Text {
                text: qsTr("版本 %1").arg(Qt.application.version)
                font.pixelSize: 12
                color: Theme.textSecondary
            }
        }
    }

    // ——————————————— 分隔线
    Rectangle {
        id: divider
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 22
        height: 1
        color: Theme.separator
    }

    // ——————————————— 信息
    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: divider.bottom
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 14
        spacing: 6

        Repeater {
            model: [
                { k: qsTr("启动内核"), v: qsTr("mcl 自研内核") },
                { k: qsTr("平台"), v: Qt.platform.os },
                { k: qsTr("界面"), v: qsTr("Qt 6 · QML 全自绘") }
            ]

            delegate: Row {
                required property var modelData
                width: parent.width
                spacing: 12

                Text {
                    width: 76
                    text: modelData.k
                    horizontalAlignment: Text.AlignRight
                    font.pixelSize: 11
                    color: Theme.textTertiary
                }
                Text {
                    width: parent.width - 88
                    text: modelData.v
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    elide: Text.ElideMiddle
                }
            }
        }
    }

    // ——————————————— 底部按钮
    Rectangle {
        id: detailsButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 16
        anchors.bottomMargin: 14
        width: detailsLabel.width + 26
        height: 26
        radius: 7
        color: detailsMouse.containsMouse
               ? (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(0, 0, 0, 0.07))
               : (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(1, 1, 1, 0.55))
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
