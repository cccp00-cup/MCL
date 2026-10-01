import QtQuick

// 市场视图：模组市场和整合包市场共用。
// 差别只有 client（两个独立实例）和 kind（"mod" / "modpack"）。
Item {
    id: view

    property var client          // ModrinthClient
    property string kind: "mod"
    // 筛选条件（空 = 不限）
    property string versionFilter: ""
    property string loaderFilter: ""
    signal installRequested(var project)

    // 外面改了筛选（例如从实例设置进来时预填了该实例的 MC 版本），
    // 把输入框同步过来。用户自己打字时不会走到这儿 —— 那时 versionFilter 还没变。
    onVersionFilterChanged: versionInput.text = view.versionFilter

    // 安装进度（0~1）。内核的阶段和进度都推过来，这里存一份给进度条绑定。
    property real progress: 0
    readonly property string stage: kernel.currentStage
    readonly property bool failed: stage.indexOf("失败") >= 0
    // "完成/失败"是终态：要露一下让用户知道结果，但不该一直挂在那儿
    readonly property bool installing: stage !== "" && !failed && stage.indexOf("完成") < 0

    Connections {
        target: kernel
        function onDownloadProgress(done, total, bytes, bytesTotal) {
            if (bytesTotal > 0)
                view.progress = Math.min(1, bytes / bytesTotal)
            else if (total > 0)
                view.progress = done / total
        }
        function onStageChanged(stage) {
            if (stage.indexOf("完成") >= 0 || stage.indexOf("失败") >= 0)
                view.progress = 0
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    // ——————————————————— 顶部搜索栏
    Rectangle {
        id: searchBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        height: 34
        radius: 8
        color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05)
        border.width: 1
        border.color: Theme.separator

        TextInput {
            id: query
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font.pixelSize: 13
            selectByMouse: true
            clip: true
            onAccepted: view.search(query.text)
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.left: parent.left
            anchors.leftMargin: 12
            visible: query.text === "" && !query.activeFocus
            text: view.kind === "modpack" ? qsTr("搜索整合包…") : qsTr("搜索模组…")
            color: Theme.textTertiary
            font.pixelSize: 13
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 12
            visible: view.client && view.client.loading
            text: qsTr("检索中…")
            color: Theme.textTertiary
            font.pixelSize: 11
        }

        // 从「实例设置」进来的话，这里会显示绑定的实例，装模组时不用再问
        Text {
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 64
            visible: view.client && view.client.targetInstance !== "" && view.kind === "mod"
            text: qsTr("装入：%1").arg(kernel.instanceName(view.client.targetInstance))
            color: Theme.accent
            font.pixelSize: 11
        }
    }

    // ——————————————————— 筛选行
    Row {
        id: filterRow
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: searchBar.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 8
        height: 24
        spacing: 8
        // 四个市场（模组/整合包/资源包/光影）都按 MC 版本筛。
        // 版本这东西哪个市场都躲不开 —— 挑了个 1.20.1 的资源包装到 1.21 上照样不生效。

        component FilterChip: Rectangle {
            id: chip
            property string label: ""
            property string value: ""
            width: chipLabel.width + 20
            height: 24
            radius: 6
            color: chip.value !== ""
                   ? Qt.rgba(0.04, 0.52, 1.0, 0.18)
                   : (chipMouse.containsMouse ? Theme.hoverFill : Qt.rgba(0, 0, 0, 0.04))
            border.width: 1
            border.color: chip.value !== "" ? Theme.accent : Theme.separator

            Text {
                id: chipLabel
                anchors.centerIn: parent
                text: chip.label
                font.pixelSize: 11
                color: chip.value !== "" ? Theme.accent : Theme.textSecondary
            }
            MouseArea {
                id: chipMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: chip.menuRequested()
            }
            signal menuRequested()
        }

        // 版本：**可以直接手输**，也可以点右边的小箭头从版本清单里挑。
        //
        // 以前这里是个只能选的 chip，而菜单只列了前 10 个版本 —— 想筛到
        // 1.20.1 之外的版本基本没戏。版本号本来就该让人自己打。
        Rectangle {
            id: versionField
            width: 124
            height: 24
            radius: 6
            color: versionInput.text.trim() !== ""
                   ? Qt.rgba(0.04, 0.52, 1.0, 0.18)
                   : (versionHover.containsMouse ? Theme.hoverFill : Qt.rgba(0, 0, 0, 0.04))
            border.width: 1
            border.color: versionInput.text.trim() !== "" ? Theme.accent : Theme.separator

            // 回车或失焦时提交。**不能**用 text 绑定 view.versionFilter ——
            // 赋值会打断绑定，之后外部再改就同步不回来了。
            function commit() {
                const v = versionInput.text.trim()
                if (v === view.versionFilter)
                    return
                view.versionFilter = v
                view.search(query.text)
            }

            TextInput {
                id: versionInput
                anchors.left: parent.left
                anchors.right: dropdownArrow.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                verticalAlignment: TextInput.AlignVCenter
                font.pixelSize: 11
                color: Theme.textPrimary
                selectionColor: Theme.accent
                selectedTextColor: "#ffffff"
                clip: true
                inputMethodHints: Qt.ImhNoPredictiveText
                onAccepted: versionField.commit()
                onActiveFocusChanged: if (!activeFocus) versionField.commit()

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("版本（可输入）")
                    font.pixelSize: 11
                    color: Theme.textTertiary
                    visible: versionInput.text === "" && !versionInput.activeFocus
                }
            }

            Text {
                id: dropdownArrow
                anchors.right: parent.right
                anchors.rightMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                text: "⌄"
                font.pixelSize: 11
                color: Theme.textTertiary

                MouseArea {
                    anchors.fill: parent
                    anchors.margins: -7
                    cursorShape: Qt.PointingHandCursor
                    onClicked: versionMenu.open()
                }
            }

            // 只负责悬浮高亮，不接点击（TextInput 得能拿到焦点）
            MouseArea {
                id: versionHover
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        // 加载器只有模组和整合包有意义 —— 资源包和光影没有这个概念
        FilterChip {
            visible: view.kind === "mod" || view.kind === "modpack"
            label: view.loaderFilter === "" ? qsTr("加载器：不限") : view.loaderFilter
            value: view.loaderFilter
            onMenuRequested: loaderMenu.open()
        }

        McMenu {
            id: versionMenu
            dark: true
            entries: {
                const out = [{ text: qsTr("不限"), action: "" }]
                const list = kernel.availableVersions
                // 清单有九百多条，全塞进菜单不现实；列前 40 个覆盖常用版本，
                // 其余的让用户直接在输入框里打。
                for (let i = 0; i < list.length && i < 40; ++i)
                    out.push({ text: list[i].name, action: list[i].id })
                return out
            }
            onTriggered: function (v) {
                view.versionFilter = v
                view.search(query.text)
            }
        }

        McMenu {
            id: loaderMenu
            dark: true
            entries: [
                { text: qsTr("不限"), action: "" },
                { text: "Fabric", action: "fabric" },
                { text: "Forge", action: "forge" },
                { text: "NeoForge", action: "neoforge" },
                { text: "Quilt", action: "quilt" }
            ]
            onTriggered: function (v) {
                view.loaderFilter = v
                view.search(query.text)
            }
        }
    }

    // ——————————————————— 结果列表
    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: filterRow.visible ? filterRow.bottom : searchBar.bottom
        anchors.bottom: statusBar.top
        anchors.margins: 14
        anchors.topMargin: 8
        clip: true
        spacing: 6
        model: view.client ? view.client.results : []

        delegate: Rectangle {
            required property var modelData
            width: list.width
            height: 68
            radius: 9
            color: rowMouse.containsMouse ? Theme.hoverFill : Qt.rgba(0, 0, 0, 0.03)

            Image {
                id: icon
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 44
                height: 44
                source: modelData.icon
                sourceSize: Qt.size(44, 44)
                fillMode: Image.PreserveAspectFit
                smooth: true

                // 没有图标时用个方块顶上
                Rectangle {
                    anchors.fill: parent
                    visible: parent.status !== Image.Ready
                    radius: 8
                    color: Qt.rgba(0.4, 0.35, 0.6, 0.5)
                }
            }

            Column {
                anchors.left: icon.right
                anchors.leftMargin: 12
                anchors.right: actionButton.left
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3

                Text {
                    width: parent.width
                    text: modelData.title
                    elide: Text.ElideRight
                    font.pixelSize: 13
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                }
                Text {
                    width: parent.width
                    text: modelData.description
                    elide: Text.ElideRight
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
                Text {
                    width: parent.width
                    text: qsTr("%1 · %2 次下载")
                              .arg(modelData.author)
                              .arg(modelData.downloads)
                    elide: Text.ElideRight
                    font.pixelSize: 10
                    color: Theme.textTertiary
                }
            }

            Rectangle {
                id: actionButton
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 72
                height: 26
                radius: 6
                color: actionMouse.containsMouse ? Qt.darker(Theme.accent, 1.1) : Theme.accent

                Text {
                    anchors.centerIn: parent
                    text: view.kind === "modpack" ? qsTr("安装") : qsTr("装到…")
                    font.pixelSize: 12
                    color: "white"
                }

                MouseArea {
                    id: actionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: view.installRequested(modelData)
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

    // ——————————————————— 安装进度（内核在干活时才有）
    Rectangle {
        id: statusBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        height: view.stage !== "" ? 42 : 0
        visible: view.stage !== ""
        radius: 8
        color: view.failed
               ? Qt.rgba(0.55, 0.16, 0.16, 0.55)
               : (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.05))
        border.width: 1
        border.color: view.failed ? Qt.rgba(0.9, 0.35, 0.35, 0.6) : Theme.separator

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            spacing: 5

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
                    width: parent.width * view.progress
                    height: parent.height
                    radius: 2
                    color: Theme.accent
                    visible: view.installing
                }
            }
        }
    }

    // ——————————————————— 空态
    Column {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: view.installing ? -26 : 0
        spacing: 8
        visible: list.count === 0 && !(view.client && view.client.loading)

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: view.kind === "modpack" ? qsTr("整合包市场") : qsTr("模组市场")
            font.pixelSize: 18
            font.weight: Font.DemiBold
            color: Theme.textSecondary
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("在上面搜点什么，或者直接回车看最热门的")
            font.pixelSize: 12
            color: Theme.textTertiary
        }
    }

    function search(text) {
        if (!view.client)
            return
        view.client.search(view.kind, text, view.versionFilter, view.loaderFilter)
    }

    Component.onCompleted: search("")
}
