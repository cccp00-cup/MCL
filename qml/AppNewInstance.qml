import QtQuick
import QtQuick.Controls

// 「新建实例」应用。
//
// 版本清单有九百多条 —— 桌面右键那种弹出菜单根本装不下（原来还只挑了 12 条正式版），
// 所以单独开一个窗口：类型筛选 + 搜索 + 完整列表 + 加载器选择。
Item {
    id: root

    property string selectedId: ""
    property string typeFilter: "release"     // release | snapshot | old | all
    property string loaderId: "vanilla"       // vanilla | fabric | forge | neoforge
    property string searchText: ""

    readonly property var allVersions: kernel.availableVersions

    // —— 安装进度。点「创建」之后要下 client.jar 和一堆库，几十上百 MB，
    // 没有反馈的话用户只会觉得按钮点坏了。
    property real progress: 0
    // 记录是哪个版本在装 —— 建完之后要能认出来
    property string creatingId: ""
    readonly property string stage: kernel.currentStage
    readonly property bool failed: stage.indexOf("失败") >= 0
    readonly property bool installing: creatingId !== "" && stage !== ""
                                        && !failed && stage.indexOf("完成") < 0

    Connections {
        target: kernel
        function onDownloadProgress(done, total, bytes, bytesTotal) {
            if (bytesTotal > 0)
                root.progress = Math.min(1, bytes / bytesTotal)
            else if (total > 0)
                root.progress = done / total
        }
        function onStageChanged(stage) {
            if (stage.indexOf("失败") >= 0)
                root.creatingId = ""     // 失败就把按钮放回去，让用户能重试
        }
        // 实例建好了：清掉进度，按钮恢复（窗口留着，方便连着建几个）
        function onInstanceCreated(instanceId) {
            if (instanceId === "")
                return
            root.creatingId = ""
            root.progress = 0
        }
    }

    // 筛选 + 搜索。九百来条的 JS 循环是微秒级，不值得做增量。
    readonly property var shown: {
        const out = []
        const needle = searchText.trim().toLowerCase()
        const list = allVersions
        for (let i = 0; i < list.length; ++i) {
            const v = list[i]
            if (typeFilter === "release" && v.type !== "release")
                continue
            if (typeFilter === "snapshot" && v.type !== "snapshot")
                continue
            if (typeFilter === "old" && v.type !== "old_beta" && v.type !== "old_alpha")
                continue
            if (needle !== "" && v.id.toLowerCase().indexOf(needle) < 0)
                continue
            out.push(v)
        }
        return out
    }

    // 列表一变就自动选中第一条，省得用户还得先点一下才能建
    onShownChanged: {
        if (shown.length === 0) {
            selectedId = ""
            return
        }
        let stillThere = false
        for (let i = 0; i < shown.length; ++i) {
            if (shown[i].id === selectedId) {
                stillThere = true
                break
            }
        }
        if (!stillThere)
            selectedId = shown[0].id
    }

    function typeLabel(t) {
        if (t === "release")
            return qsTr("正式版")
        if (t === "snapshot")
            return qsTr("快照")
        if (t === "old_beta")
            return qsTr("Beta")
        if (t === "old_alpha")
            return qsTr("Alpha")
        return t
    }

    function create() {
        if (selectedId === "" || root.installing)
            return
        root.creatingId = selectedId
        root.progress = 0
        const label = nameField.text.trim()
        if (loaderId === "fabric") {
            kernel.createFabricInstance(selectedId, label)
        } else if (loaderId === "forge" || loaderId === "neoforge") {
            kernel.createModdedInstance(selectedId, loaderId, label)
        } else {
            kernel.createInstance(selectedId, label)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    // ——————————————————— 顶部表单
    Column {
        id: form
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 18
        spacing: 12

        // 实例名
        Row {
            spacing: 10
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 52
                text: qsTr("名称")
                font.pixelSize: 12
                color: Theme.textSecondary
            }
            Rectangle {
                width: 260
                height: 26
                radius: 6
                color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05)
                border.width: 1
                border.color: nameField.activeFocus ? Theme.accent : Theme.separator
                TextInput {
                    id: nameField
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 12
                    color: Theme.textPrimary
                    selectionColor: Theme.accent
                    selectedTextColor: "#ffffff"
                    clip: true
                    // 留空就用版本号当名字，内核那边会兜
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("留空则用版本号")
                        font.pixelSize: 12
                        color: Theme.textTertiary
                        visible: nameField.text === "" && !nameField.activeFocus
                    }
                }
            }
        }

        // 加载器
        Row {
            spacing: 10
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 52
                text: qsTr("加载器")
                font.pixelSize: 12
                color: Theme.textSecondary
            }
            Repeater {
                model: [
                    { id: "vanilla", label: qsTr("原版") },
                    { id: "fabric", label: qsTr("Fabric") },
                    { id: "forge", label: qsTr("Forge") },
                    { id: "neoforge", label: qsTr("NeoForge") }
                ]
                delegate: Rectangle {
                    width: chipText.width + 20
                    height: 26
                    radius: 6
                    color: root.loaderId === modelData.id
                           ? Theme.accent
                           : (loaderMouse.containsMouse ? Theme.hoverFill : "transparent")
                    border.width: 1
                    border.color: root.loaderId === modelData.id ? Theme.accent : Theme.separator
                    Text {
                        id: chipText
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: 12
                        color: root.loaderId === modelData.id ? "#ffffff" : Theme.textPrimary
                    }
                    MouseArea {
                        id: loaderMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.loaderId = modelData.id
                    }
                }
            }
        }

        // 版本类型 + 搜索
        Row {
            spacing: 10
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 52
                text: qsTr("版本")
                font.pixelSize: 12
                color: Theme.textSecondary
            }
            Repeater {
                model: [
                    { id: "release", label: qsTr("正式版") },
                    { id: "snapshot", label: qsTr("快照") },
                    { id: "old", label: qsTr("远古") },
                    { id: "all", label: qsTr("全部") }
                ]
                delegate: Rectangle {
                    width: typeText.width + 18
                    height: 26
                    radius: 6
                    color: root.typeFilter === modelData.id
                           ? Theme.accent
                           : (typeMouse.containsMouse ? Theme.hoverFill : "transparent")
                    border.width: 1
                    border.color: root.typeFilter === modelData.id ? Theme.accent : Theme.separator
                    Text {
                        id: typeText
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: 12
                        color: root.typeFilter === modelData.id ? "#ffffff" : Theme.textPrimary
                    }
                    MouseArea {
                        id: typeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.typeFilter = modelData.id
                    }
                }
            }

            Rectangle {
                width: 150
                height: 26
                radius: 6
                color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05)
                border.width: 1
                border.color: searchField.activeFocus ? Theme.accent : Theme.separator
                TextInput {
                    id: searchField
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 12
                    color: Theme.textPrimary
                    selectionColor: Theme.accent
                    selectedTextColor: "#ffffff"
                    clip: true
                    onTextChanged: root.searchText = text
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("搜索版本号")
                        font.pixelSize: 12
                        color: Theme.textTertiary
                        visible: searchField.text === "" && !searchField.activeFocus
                    }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("%1 个版本").arg(root.shown.length)
                font.pixelSize: 11
                color: Theme.textTertiary
            }
        }
    }

    // ——————————————————— 版本列表
    Rectangle {
        id: listFrame
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: form.bottom
        anchors.bottom: footer.top
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.topMargin: 14
        anchors.bottomMargin: 12
        radius: 8
        color: shellSettings.darkMode ? Qt.rgba(0, 0, 0, 0.18) : Qt.rgba(0, 0, 0, 0.03)
        border.width: 1
        border.color: Theme.separator
        clip: true

        ListView {
            id: versionList
            anchors.fill: parent
            anchors.margins: 4
            model: root.shown
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                required property var modelData
                width: versionList.width
                height: 30
                radius: 5
                color: root.selectedId === modelData.id
                       ? Theme.accent
                       : (rowMouse.containsMouse ? Theme.hoverFill : "transparent")

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.id
                    font.pixelSize: 12
                    font.weight: root.selectedId === modelData.id ? Font.DemiBold : Font.Normal
                    color: root.selectedId === modelData.id ? "#ffffff" : Theme.textPrimary
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.typeLabel(modelData.type) + "   " + modelData.released
                    font.pixelSize: 11
                    color: root.selectedId === modelData.id ? Qt.rgba(1, 1, 1, 0.75)
                                                            : Theme.textTertiary
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.selectedId = modelData.id
                    onDoubleClicked: {
                        root.selectedId = modelData.id
                        root.create()
                    }
                }
            }
        }

        // 清单还在路上：给个转圈，别让用户对着空白列表发呆
        Column {
            anchors.centerIn: parent
            spacing: 12
            visible: root.allVersions.length === 0

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 28
                height: 28

                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 900
                    loops: Animation.Infinite
                    running: root.allVersions.length === 0
                }

                // 自绘的转圈：一圈小点，透明度沿着圆周渐变
                Repeater {
                    model: 8
                    delegate: Rectangle {
                        required property int index
                        width: 3.5
                        height: 3.5
                        radius: 1.75
                        color: Theme.textTertiary
                        opacity: 0.12 + 0.88 * (index / 8.0)
                        x: 14 + 10 * Math.sin(index / 8.0 * Math.PI * 2) - 1.75
                        y: 14 - 10 * Math.cos(index / 8.0 * Math.PI * 2) - 1.75
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("正在获取版本清单…")
                font.pixelSize: 12
                color: Theme.textTertiary
            }
        }

        Text {
            anchors.centerIn: parent
            visible: root.allVersions.length > 0 && root.shown.length === 0
            text: qsTr("没有匹配的版本")
            font.pixelSize: 12
            color: Theme.textTertiary
        }
    }

    // ——————————————————— 底部
    Row {
        id: footer
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 20
        anchors.bottomMargin: 16
        spacing: 10

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: {
                if (root.installing)
                    return root.stage
                if (root.failed)
                    return root.stage
                if (root.selectedId === "")
                    return qsTr("选一个版本")
                return qsTr("将创建：%1").arg(root.selectedId)
            }
            font.pixelSize: 11
            color: root.failed ? "#c05050" : Theme.textTertiary
        }

        Rectangle {
            id: createButton
            width: 108
            height: 30
            radius: 7
            color: createMouse.containsMouse && root.selectedId !== "" && !root.installing
                   ? Qt.darker(Theme.accent, 1.1) : Theme.accent
            opacity: root.selectedId === "" ? 0.4 : 1.0
            clip: true

            // 进度就画在按钮里：底色上盖一层亮色，从左往右长。
            // 放在按钮内部而不是旁边，是为了让「进度」和「点下去的东西」是同一个。
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: parent.width * root.progress
                color: Qt.rgba(1, 1, 1, 0.32)
                visible: root.installing
            }

            Text {
                anchors.centerIn: parent
                text: root.installing
                      ? qsTr("%1%").arg(Math.round(root.progress * 100))
                      : qsTr("创建")
                font.pixelSize: 12
                font.weight: Font.DemiBold
                color: "#ffffff"
            }
            MouseArea {
                id: createMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: root.selectedId !== "" && !root.installing
                cursorShape: Qt.PointingHandCursor
                onClicked: root.create()
            }
        }
    }
}
