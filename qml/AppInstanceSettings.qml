import QtQuick

// 「实例设置」：某个实例的名字、版本、以及它的模组清单。
//
// 模组那一块是这个窗口的重点 —— 可以启用/停用、移除，也可以点「添加模组…」
// 直接跳到模组市场（会把市场绑到这个实例上，装的时候不用再选装到哪）。
Item {
    id: root

    // 当前编辑的实例（由打开窗口的一方通过 desktop.editingInstance 设好）
    readonly property string instanceId: desktop.editingInstance
    readonly property var info: instanceId !== "" ? kernel.instanceInfo(instanceId) : ({})

    property var mods: []
    // fileName -> { versionNumber, newFileName }
    property var updates: ({})
    property string updateHint: ""

    // Java 那一行显示什么：没指定就说"自动挑选"，指定了就把厂商和版本列出来
    readonly property string javaLabel: {
        const pinned = root.info.javaPath !== undefined ? root.info.javaPath : ""
        if (pinned === "")
            return qsTr("自动挑选（按版本要求）")
        const list = kernel.availableJava()
        for (let i = 0; i < list.length; ++i)
            if (list[i].path === pinned)
                return list[i].label
        return qsTr("指定的 Java 已经不在了")
    }

    function refreshMods() {
        mods = instanceId !== "" ? kernel.instanceMods(instanceId) : []
    }

    Component.onCompleted: refreshMods()

    Connections {
        target: kernel
        function onContentUpdatesReady(instanceId, updates) {
            if (instanceId !== root.instanceId)
                return
            const map = ({})
            for (let i = 0; i < updates.length; ++i)
                map[updates[i].fileName] = updates[i]
            root.updates = map
            root.updateHint = updates.length === 0 ? qsTr("都是最新版")
                                                   : qsTr("%1 个可更新").arg(updates.length)
        }
        function onContentUpdatesFailed(instanceId, error) {
            if (instanceId === root.instanceId)
                root.updateHint = error
        }
    }

    Connections {
        target: kernel
        function onInstanceModsChanged(changed) {
            if (changed === root.instanceId)
                root.refreshMods()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 18
        spacing: 4

        // —— 图标 + 加载器
        Row {
            width: parent.width
            spacing: 12
            bottomPadding: 10

            Rectangle {
                width: 54
                height: 54
                radius: 13
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.lighter("#3f8f5f", 1.25) }
                    GradientStop { position: 1.0; color: Qt.darker("#3f8f5f", 1.15) }
                }
                Image {
                    anchors.centerIn: parent
                    width: 30
                    height: 30
                    source: root.info.icon !== undefined && root.info.icon !== ""
                            ? root.info.icon : "qrc:/mcl/icons/grass-block.svg"
                    sourceSize: Qt.size(30, 30)
                    fillMode: Image.PreserveAspectFit
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: root.info.name !== undefined ? root.info.name : ""
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: (root.info.versionId !== undefined ? root.info.versionId : "—")
                          + " · " + (root.info.loader !== undefined ? root.info.loader : "—")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
            }
        }

        // —— 名字
        Text {
            text: qsTr("名称")
            font.pixelSize: 11
            color: Theme.textSecondary
        }
        Rectangle {
            width: parent.width
            height: 30
            radius: 7
            color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.04)
            border.width: 1
            border.color: Theme.separator

            TextInput {
                id: nameInput
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.textPrimary
                font.pixelSize: 13
                selectByMouse: true
                clip: true
                text: root.info.name !== undefined ? root.info.name : ""
                onAccepted: kernel.renameInstance(root.instanceId, text)
            }
        }

        // —— 目录
        Row {
            topPadding: 6
            spacing: 8
            width: parent.width

            Text {
                text: root.info.dir !== undefined ? root.info.dir : ""
                width: parent.width - 160
                elide: Text.ElideMiddle
                font.pixelSize: 11
                color: Theme.textTertiary
            }
        }

        // —— Java（自己挑厂商是玩 MC 的基本功，所以这里不做自动下载，只提供选择）
        Row {
            topPadding: 12
            spacing: 8
            width: parent.width

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Java")
                font.pixelSize: 11
                color: Theme.textSecondary
            }

            Rectangle {
                width: 260
                height: 26
                radius: 6
                color: javaMouse.containsMouse ? Theme.hoverFill : Qt.rgba(0, 0, 0, 0.04)
                border.width: 1
                border.color: Theme.separator

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.javaLabel
                    elide: Text.ElideRight
                    font.pixelSize: 12
                    color: Theme.textPrimary
                }

                MouseArea {
                    id: javaMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: javaMenu.open()
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("本机扫到 %1 个").arg(kernel.availableJava().length)
                font.pixelSize: 10
                color: Theme.textTertiary
            }
        }

        McMenu {
            id: javaMenu
            dark: true
            entries: {
                const out = [{ text: qsTr("自动挑选（按版本要求）"), action: "" }]
                const list = kernel.availableJava()
                for (let i = 0; i < list.length; ++i)
                    out.push({ text: list[i].label + "   " + list[i].path, action: list[i].path })
                if (list.length === 0)
                    out.push({ text: qsTr("没扫到任何 Java"), action: "", enabled: false })
                return out
            }
            onTriggered: function (path) {
                kernel.setInstanceJava(root.instanceId, path)
            }
        }

        // —— 模组标题 + 两个按钮
        Row {
            topPadding: 14
            spacing: 8
            width: parent.width

            Text {
                text: qsTr("模组（%1）").arg(root.mods.length)
                font.pixelSize: 13
                font.weight: Font.DemiBold
                color: Theme.textPrimary
            }

            Item { width: parent.width - 320; height: 1 }

            Rectangle {
                width: 88
                height: 24
                radius: 6
                color: addMouse.containsMouse ? Qt.darker(Theme.accent, 1.1) : Theme.accent
                Text {
                    anchors.centerIn: parent
                    text: qsTr("添加模组…")
                    font.pixelSize: 12
                    color: "white"
                }
                MouseArea {
                    id: addMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        // 把市场绑到这个实例上，再打开
                        modMarket.targetInstance = root.instanceId
                        desktop.openApp("mods")
                    }
                }
            }

            Rectangle {
                width: 80
                height: 24
                radius: 6
                color: checkMouse.containsMouse ? Theme.hoverFill : "transparent"
                border.width: 1
                border.color: Theme.separator
                Text {
                    anchors.centerIn: parent
                    text: qsTr("检查更新")
                    font.pixelSize: 12
                    color: Theme.textPrimary
                }
                MouseArea {
                    id: checkMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.updateHint = qsTr("检查中…")
                        kernel.checkContentUpdates(root.instanceId)
                    }
                }
            }

            Rectangle {
                width: 96
                height: 24
                radius: 6
                color: folderMouse.containsMouse ? Theme.hoverFill : "transparent"
                border.width: 1
                border.color: Theme.separator
                Text {
                    anchors.centerIn: parent
                    text: qsTr("打开目录")
                    font.pixelSize: 12
                    color: Theme.textPrimary
                }
                MouseArea {
                    id: folderMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: kernel.openInstanceFolder(root.instanceId)
                }
            }
        }
    }

    // —— 模组列表
    ListView {
        id: modList
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 18
        anchors.topMargin: 290
        clip: true
        spacing: 3
        model: root.mods

        delegate: Rectangle {
            required property var modelData
            width: modList.width
            height: 38
            radius: 7
            color: rowMouse.containsMouse ? Theme.hoverFill : Qt.rgba(0, 0, 0, 0.025)

            // 停用的模组整行暗淡
            opacity: modelData.enabled ? 1.0 : 0.5

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1

                Row {
                    spacing: 6
                    Text {
                        width: modList.width - 240
                        text: modelData.displayName
                        elide: Text.ElideMiddle
                        font.pixelSize: 12
                        color: Theme.textPrimary
                    }
                    Rectangle {
                        visible: root.updates[modelData.fileName] !== undefined
                        width: 52
                        height: 15
                        radius: 4
                        color: Qt.rgba(0.04, 0.64, 0.42, 0.22)
                        Text {
                            anchors.centerIn: parent
                            text: root.updates[modelData.fileName] !== undefined
                                  ? root.updates[modelData.fileName].versionNumber
                                  : ""
                            font.pixelSize: 9
                            color: "#2fa36b"
                        }
                    }
                }
                Text {
                    text: modelData.enabled ? modelData.sizeText : qsTr("已停用 · %1").arg(modelData.sizeText)
                    font.pixelSize: 10
                    color: Theme.textTertiary
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Rectangle {
                    width: 52
                    height: 22
                    radius: 5
                    color: toggleMouse.containsMouse ? Theme.hoverFill : "transparent"
                    border.width: 1
                    border.color: Theme.separator
                    Text {
                        anchors.centerIn: parent
                        text: modelData.enabled ? qsTr("停用") : qsTr("启用")
                        font.pixelSize: 11
                        color: Theme.textPrimary
                    }
                    MouseArea {
                        id: toggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: kernel.toggleInstanceMod(root.instanceId, modelData.fileName)
                    }
                }

                Rectangle {
                    width: 52
                    height: 22
                    radius: 5
                    color: removeMouse.containsMouse ? Qt.rgba(0.7, 0.2, 0.2, 0.85) : Qt.rgba(0, 0, 0, 0.05)
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("移除")
                        font.pixelSize: 11
                        color: removeMouse.containsMouse ? "white" : Theme.textPrimary
                    }
                    MouseArea {
                        id: removeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: kernel.removeInstanceMod(root.instanceId, modelData.fileName)
                    }
                }
            }

            MouseArea {
                id: rowMouse
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.rightMargin: 120
                hoverEnabled: true
            }
        }
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: 40
        visible: root.mods.length === 0
        text: qsTr("这个实例还没有模组 —— 点「添加模组…」去市场看看")
        font.pixelSize: 12
        color: Theme.textTertiary
    }
}
