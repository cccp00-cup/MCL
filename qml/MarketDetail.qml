import QtQuick

// 市场详情页：项目信息 + 全部版本，版本由用户自己挑。
//
// 为什么要摊开让人选：Modrinth 返回的版本列表**不保证**按"跟你的实例对得上"排，
// 内核以前直接取 `versions.first()`，很容易装到不兼容的版本（游戏直接起不来）。
// 现在把版本列出来，并且把匹配当前实例 MC 版本 + 加载器的那些标成可选，
// 其余灰掉但仍然可见 —— 用户想装到别处也点得动。
Item {
    id: root

    property var client                    // ModrinthClient
    property var project                   // 搜索结果里的那个 object
    property string kind: "mod"
    // 当前实例的 MC 版本和加载器（小写）。没绑实例时都为空 = 不判断适配
    property string instanceGameVersion: ""
    property string instanceLoader: ""

    signal installRequested(var version)
    signal backRequested()

    readonly property var versions: client ? client.projectVersions : []
    readonly property bool loading: client ? client.versionsLoading : false
    // 默认只看适配的 —— 免得列表里一堆点不了的
    property bool onlyMatching: true

    // 进页面就拉版本列表
    Component.onCompleted: {
        if (client && project)
            client.fetchVersions(project.id)
    }

    function matches(version) {
        if (instanceGameVersion !== "") {
            if (version.gameVersions.indexOf(instanceGameVersion) < 0)
                return false
        }
        if (instanceLoader !== "") {
            let ok = false
            for (let i = 0; i < version.loaders.length; ++i) {
                if (String(version.loaders[i]).toLowerCase() === instanceLoader) {
                    ok = true
                    break
                }
            }
            if (!ok)
                return false
        }
        return true
    }

    // 过滤后的列表（不适配的标灰但不隐藏，除非勾了"只看适配"）
    readonly property var shown: {
        const out = []
        const list = versions
        for (let i = 0; i < list.length; ++i) {
            const v = list[i]
            const ok = matches(v)
            if (root.onlyMatching && !ok)
                continue
            const item = {}
            for (const k in v)
                item[k] = v[k]
            item.matched = ok
            out.push(item)
        }
        return out
    }

    readonly property int matchCount: {
        let n = 0
        for (let i = 0; i < versions.length; ++i)
            if (matches(versions[i]))
                n++
        return n
    }

    function formatCount(n) {
        if (n >= 1000000)
            return (n / 1000000).toFixed(1) + "M"
        if (n >= 1000)
            return (n / 1000).toFixed(1) + "K"
        return String(n)
    }

    function formatSize(bytes) {
        if (bytes >= 1024 * 1024)
            return (bytes / 1024 / 1024).toFixed(1) + " MB"
        if (bytes >= 1024)
            return Math.round(bytes / 1024) + " KB"
        return bytes + " B"
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    // ——————————————————— 顶部：返回 + 项目信息
    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 132
        color: "transparent"

        // 返回
        Rectangle {
            id: backButton
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 16
            anchors.topMargin: 12
            width: backLabel.width + 24
            height: 24
            radius: 6
            color: backMouse.containsMouse ? Theme.hoverFill : Qt.rgba(0, 0, 0, 0.04)
            border.width: 1
            border.color: Theme.separator
            Text {
                id: backLabel
                anchors.centerIn: parent
                text: qsTr("‹ 返回")
                font.pixelSize: 11
                color: Theme.textPrimary
            }
            MouseArea {
                id: backMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.backRequested()
            }
        }

        // 图标
        Rectangle {
            id: iconBox
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.top: backButton.bottom
            anchors.topMargin: 12
            width: 56
            height: 56
            radius: 12
            color: Qt.rgba(0, 0, 0, 0.05)

            Image {
                anchors.fill: parent
                anchors.margins: 6
                source: root.project ? root.project.icon : ""
                sourceSize: Qt.size(44, 44)
                fillMode: Image.PreserveAspectFit
                smooth: true
                visible: status === Image.Ready
            }
            Text {
                anchors.centerIn: parent
                visible: !root.project || !root.project.icon
                text: qsTr("?")
                font.pixelSize: 20
                color: Theme.textTertiary
            }
        }

        // 标题 / 作者 / 简介
        Column {
            anchors.left: iconBox.right
            anchors.leftMargin: 12
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.top: iconBox.top
            spacing: 4

            Text {
                width: parent.width
                text: root.project ? root.project.title : ""
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: {
                    if (!root.project)
                        return ""
                    return qsTr("%1 · %2 次下载")
                        .arg(root.project.author || qsTr("未知作者"),
                             root.formatCount(root.project.downloads || 0))
                }
                font.pixelSize: 11
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
            Text {
                width: parent.width
                text: root.project ? (root.project.description || "") : ""
                font.pixelSize: 11
                color: Theme.textTertiary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
        }
    }

    // ——————————————————— 版本标题行
    Row {
        id: versionHeader
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 24
        spacing: 10

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("版本")
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.instanceGameVersion !== ""
            text: qsTr("适配当前实例的有 %1 个").arg(root.matchCount)
            font.pixelSize: 11
            color: Theme.textTertiary
        }

        // 只看适配的
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: filterLabel.width + 20
            height: 22
            radius: 6
            visible: root.instanceGameVersion !== ""
            color: root.onlyMatching ? Qt.rgba(0.04, 0.52, 1.0, 0.18)
                                     : (filterMouse.containsMouse ? Theme.hoverFill
                                                                  : Qt.rgba(0, 0, 0, 0.04))
            border.width: 1
            border.color: root.onlyMatching ? Theme.accent : Theme.separator
            Text {
                id: filterLabel
                anchors.centerIn: parent
                text: qsTr("只看适配的")
                font.pixelSize: 11
                color: root.onlyMatching ? Theme.accent : Theme.textSecondary
            }
            MouseArea {
                id: filterMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.onlyMatching = !root.onlyMatching
            }
        }
    }

    // ——————————————————— 版本列表
    Rectangle {
        id: listFrame
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: versionHeader.bottom
        anchors.bottom: parent.bottom
        anchors.margins: 16
        anchors.topMargin: 6
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
            spacing: 2

            delegate: Rectangle {
                required property var modelData
                width: versionList.width
                height: 44
                radius: 6
                // 不适配的整行压暗，但不隐藏 —— 用户可能就想装到别处
                opacity: modelData.matched ? 1.0 : 0.5
                color: rowMouse.containsMouse ? Theme.hoverFill : "transparent"

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    Text {
                        text: modelData.name && modelData.name !== ""
                              ? modelData.name
                              : modelData.versionNumber
                        font.pixelSize: 12
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        width: versionList.width - 200
                    }
                    Text {
                        text: qsTr("%1  ·  %2  ·  %3")
                            .arg(modelData.gameVersionsText,
                                 modelData.loadersText,
                                 modelData.datePublished)
                        font.pixelSize: 10
                        color: Theme.textTertiary
                        elide: Text.ElideRight
                        width: versionList.width - 200
                    }
                }

                // 「适配」标记
                Rectangle {
                    anchors.right: installButton.left
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    visible: modelData.matched && root.instanceGameVersion !== ""
                    width: matchLabel.width + 12
                    height: 18
                    radius: 4
                    color: Qt.rgba(0.13, 0.65, 0.35, 0.18)
                    Text {
                        id: matchLabel
                        anchors.centerIn: parent
                        text: qsTr("适配")
                        font.pixelSize: 10
                        color: "#21a35a"
                    }
                }

                Rectangle {
                    id: installButton
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 56
                    height: 24
                    radius: 6
                    color: installMouse.containsMouse ? Qt.darker(Theme.accent, 1.1)
                                                      : Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("安装")
                        font.pixelSize: 11
                        color: "#ffffff"
                    }
                    MouseArea {
                        id: installMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.installRequested(modelData)
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    anchors.rightMargin: installButton.width + 14
                    hoverEnabled: true
                    acceptedButtons: Qt.LeftButton
                    cursorShape: Qt.ArrowCursor
                }
            }
        }

        // 加载中 / 空
        Column {
            anchors.centerIn: parent
            spacing: 10
            visible: root.loading || root.shown.length === 0

            Item {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 24
                height: 24
                visible: root.loading

                RotationAnimator on rotation {
                    from: 0
                    to: 360
                    duration: 900
                    loops: Animation.Infinite
                    running: root.loading
                }
                Repeater {
                    model: 8
                    delegate: Rectangle {
                        required property int index
                        width: 3
                        height: 3
                        radius: 1.5
                        color: Theme.textTertiary
                        opacity: 0.12 + 0.88 * (index / 8.0)
                        x: 12 + 8.5 * Math.sin(index / 8.0 * Math.PI * 2) - 1.5
                        y: 12 - 8.5 * Math.cos(index / 8.0 * Math.PI * 2) - 1.5
                    }
                }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.loading ? qsTr("正在取版本列表…")
                                   : (root.onlyMatching && root.versions.length > 0
                                      ? qsTr("没有适配当前实例的版本 —— 取消勾选「只看适配的」看看")
                                      : qsTr("这个项目没有可下载的版本"))
                font.pixelSize: 12
                color: Theme.textTertiary
            }
        }
    }
}
