import QtQuick

// 「启动器」应用：mcl 内嵌内核的版本列表。
//
// 这里只跟 McKernel 这个抽象打交道 —— 内核是自研的 Vanilla 实现，
// 界面完全不知道它是怎么下载、怎么拼命令行的。
Item {
    id: root

    // 下载进度（0~1）。信号推过来，这里存一份供进度条绑定
    property real progress: 0
    property bool busy: kernel.currentStage !== "" && !kernel.currentStage.startsWith("失败")

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Connections {
        target: kernel
        function onDownloadProgress(done, total, bytes, bytesTotal) {
            if (bytesTotal > 0)
                root.progress = Math.min(1, bytes / bytesTotal)
            else if (total > 0)
                root.progress = done / total
        }
        function onLaunchFailed(instanceId, error) {
            root.progress = 0
        }
        function onLaunchStarted(instanceId) {
            root.progress = 0
        }
    }

    // ——————————————————— 顶部状态条：当前阶段 + 进度
    Rectangle {
        id: banner
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        height: visible ? 56 : 0
        visible: kernel.currentStage !== ""
        radius: 8
        color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.06) : Qt.rgba(0, 0, 0, 0.05)
        border.width: 1
        border.color: Theme.separator

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 6

            Text {
                width: parent.width
                text: kernel.currentStage
                elide: Text.ElideRight
                font.pixelSize: 12
                color: Theme.textPrimary
            }

            Rectangle {
                width: parent.width
                height: 4
                radius: 2
                color: Theme.separator

                Rectangle {
                    width: parent.width * root.progress
                    height: parent.height
                    radius: 2
                    color: Theme.accent
                    visible: root.busy
                }
            }
        }
    }

    Text {
        id: sectionTitle
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: banner.bottom
        anchors.topMargin: 14
        anchors.leftMargin: 18
        text: qsTr("版本 · %1").arg(kernel.name)
        font.pixelSize: 14
        font.weight: Font.DemiBold
        color: Theme.textPrimary
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: sectionTitle.bottom
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.topMargin: 2
        text: qsTr("Java：%1").arg(kernel.javaInfo)
        elide: Text.ElideRight
        font.pixelSize: 11
        color: Theme.textTertiary
    }

    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: sectionTitle.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 24
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        anchors.bottomMargin: 12
        clip: true
        spacing: 2
        model: kernel.instances

        delegate: Rectangle {
            required property var modelData
            width: list.width
            height: 58
            radius: 8
            color: rowMouse.containsMouse ? Theme.hoverFill : "transparent"

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: modelData.name
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }
                Text {
                    text: modelData.state + "  ·  " + modelData.loader
                    font.pixelSize: 11
                    color: modelData.running ? "#2fa36b" : Theme.textSecondary
                }
            }

            Rectangle {
                id: actionButton
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 92
                height: 26
                radius: 6
                color: {
                    if (modelData.running)
                        return Qt.rgba(0.18, 0.64, 0.42, 1)
                    if (!modelData.installed)
                        return actionMouse.containsMouse ? Qt.rgba(0.42, 0.45, 0.52, 1)
                                                         : Qt.rgba(0.5, 0.53, 0.6, 1)
                    return actionMouse.containsMouse ? Qt.darker(Theme.accent, 1.1) : Theme.accent
                }

                Text {
                    anchors.centerIn: parent
                    text: modelData.running
                          ? qsTr("运行中")
                          : (modelData.installed ? qsTr("启动") : qsTr("安装并启动"))
                    font.pixelSize: 12
                    color: "white"
                }

                MouseArea {
                    id: actionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: !modelData.running
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.progress = 0
                        // 未安装时内核会自动先下载再启动
                        kernel.launchOffline(modelData.id, "Player")
                    }
                }
            }

            MouseArea {
                id: rowMouse
                anchors.left: parent.left
                anchors.right: actionButton.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                hoverEnabled: true
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: list.count === 0
        text: qsTr("正在获取版本列表…")
        color: Theme.textTertiary
        font.pixelSize: 13
    }
}
