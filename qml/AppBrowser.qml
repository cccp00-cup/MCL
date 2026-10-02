import QtQuick
import QtWebEngine

// 内置浏览器（多标签）。
//
// 默认搜索引擎是 Bing。地址栏接受两种输入：像网址就当网址打开，
// 否则拿去做搜索 —— 和常见浏览器的行为一致。
//
// **为什么必须接管 newWindowRequested**：
// QtWebEngine 默认**不处理** target="_blank" / window.open 这类新窗口请求，
// 表现就是"点了没反应"。B 站点视频、GitHub 上很多链接都属于这种，
// 所以不接管的话用户会觉得浏览器坏了 —— 其实请求根本没被响应。
// 这里统一改成"在新标签里打开"，这也正是标签页存在的意义。
//
// 这个文件 import 了 QtWebEngine，所以**只能通过字符串 source 惰性加载**
// （见 McWindow 里的说明）：没有 WebEngine 的环境下，连 Component 声明
// 都会在编译期报 "module QtWebEngine is not installed"。
Item {
    id: root

    readonly property string searchBase: "https://www.bing.com/search?q="
    readonly property string homePage: "https://www.bing.com"

    // 地址栏里的文本（跟随当前标签实际加载的地址）
    property string addressText: homePage
    property int currentIndex: 0

    // 每个标签一个 WebEngineView。跟着 ListModel 的增删自动创建/销毁。
    ListModel { id: tabModel }

    // 把用户输入变成真正要打开的地址
    function normalizeUrl(text) {
        const t = String(text).trim()
        if (t === "")
            return homePage
        if (/^[a-zA-Z][a-zA-Z0-9+.\-]*:\/\//.test(t))
            return t
        if (!/\s/.test(t) && /^[^\s/]+\.[^\s/]{2,}/.test(t))
            return "https://" + t
        return searchBase + encodeURIComponent(t)
    }

    function addTab(url) {
        const target = (url && url !== "") ? url : homePage
        tabModel.append({ tabUrl: target, tabTitle: qsTr("新标签页") })
        root.currentIndex = tabModel.count - 1
        return root.currentIndex
    }

    function closeTab(index) {
        if (index < 0 || index >= tabModel.count)
            return

        if (tabModel.count === 1) {
            // 最后一个标签不真关（那样内容区会空掉），回到首页就行
            tabModel.setProperty(0, "tabUrl", homePage)
            tabModel.setProperty(0, "tabTitle", qsTr("新标签页"))
            return
        }

        tabModel.remove(index)
        if (root.currentIndex >= tabModel.count)
            root.currentIndex = tabModel.count - 1
        else if (index < root.currentIndex)
            --root.currentIndex
    }

    // 当前标签的 WebEngineView
    function currentView() {
        return viewRepeater.itemAt(root.currentIndex)
    }

    function navigate(url) {
        const v = currentView()
        if (v)
            v.url = url
    }

    function goBack() { const v = currentView(); if (v && v.canGoBack) v.goBack() }
    function goForward() { const v = currentView(); if (v && v.canGoForward) v.goForward() }
    function reloadOrStop() {
        const v = currentView()
        if (!v)
            return
        if (v.loading)
            v.stop()
        else
            v.reload()
    }
    function loading() { const v = currentView(); return v ? v.loading : false }
    function loadProgress() { const v = currentView(); return v ? v.loadProgress : 0 }
    function canGoBack() { const v = currentView(); return v ? v.canGoBack : false }
    function canGoForward() { const v = currentView(); return v ? v.canGoForward : false }

    Component.onCompleted: {
        addTab(desktop.pendingBrowserUrl !== "" ? desktop.pendingBrowserUrl : homePage)
    }

    // 启动器里点「登录微软账户」会先设好 pendingBrowserUrl 再开这个窗口
    Connections {
        target: desktop
        function onPendingBrowserUrlChanged() {
            const target = desktop.pendingBrowserUrl
            if (target !== "")
                root.navigate(target)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Column {
        anchors.fill: parent
        spacing: 0

        // ——————————————— 标签栏
        Rectangle {
            id: tabBar
            width: parent.width
            height: 30
            color: shellSettings.darkMode ? Qt.rgba(0, 0, 0, 0.18)
                                          : Qt.rgba(0, 0, 0, 0.06)

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Repeater {
                    model: tabModel
                    delegate: Rectangle {
                        required property int index
                        required property string tabTitle

                        width: Math.min(180, Math.max(96, tabLabel.implicitWidth + 40))
                        height: 24
                        radius: 6
                        color: index === root.currentIndex
                               ? (shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.14)
                                                         : Qt.rgba(1, 1, 1, 0.90))
                               : (tabMouse.containsMouse ? Theme.hoverFill : "transparent")

                        Text {
                            id: tabLabel
                            anchors.left: parent.left
                            anchors.leftMargin: 9
                            anchors.right: closeTabButton.left
                            anchors.rightMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.tabTitle
                            font.pixelSize: 11
                            color: index === root.currentIndex ? Theme.textPrimary
                                                               : Theme.textSecondary
                            elide: Text.ElideRight
                        }

                        // 关闭这个标签
                        Rectangle {
                            id: closeTabButton
                            anchors.right: parent.right
                            anchors.rightMargin: 5
                            anchors.verticalCenter: parent.verticalCenter
                            width: 15
                            height: 15
                            radius: 7.5
                            color: closeTabMouse.containsMouse ? Theme.hoverFill : "transparent"

                            Text {
                                anchors.centerIn: parent
                                text: "✕"
                                font.pixelSize: 9
                                color: Theme.textSecondary
                            }
                            MouseArea {
                                id: closeTabMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.closeTab(index)
                            }
                        }

                        MouseArea {
                            id: tabMouse
                            anchors.left: parent.left
                            anchors.right: closeTabButton.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.currentIndex = index
                        }
                    }
                }

                // 新建标签
                Rectangle {
                    width: 22
                    height: 22
                    radius: 6
                    color: newTabMouse.containsMouse ? Theme.hoverFill : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        font.pixelSize: 15
                        color: Theme.textSecondary
                    }
                    MouseArea {
                        id: newTabMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.addTab(homePage)
                    }
                }
            }
        }

        // ——————————————— 工具栏
        Rectangle {
            id: toolbar
            width: parent.width
            height: 34
            color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.06)
                                          : Qt.rgba(0, 0, 0, 0.04)

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                // 后退
                Rectangle {
                    width: 24; height: 24; radius: 6
                    color: backMouse.containsMouse && root.canGoBack()
                           ? Theme.hoverFill : "transparent"
                    opacity: root.canGoBack() ? 1.0 : 0.35
                    Text {
                        anchors.centerIn: parent
                        text: "‹"
                        font.pixelSize: 17
                        color: Theme.textPrimary
                    }
                    MouseArea {
                        id: backMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: root.canGoBack()
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.goBack()
                    }
                }
                // 前进
                Rectangle {
                    width: 24; height: 24; radius: 6
                    color: fwdMouse.containsMouse && root.canGoForward()
                           ? Theme.hoverFill : "transparent"
                    opacity: root.canGoForward() ? 1.0 : 0.35
                    Text {
                        anchors.centerIn: parent
                        text: "›"
                        font.pixelSize: 17
                        color: Theme.textPrimary
                    }
                    MouseArea {
                        id: fwdMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        enabled: root.canGoForward()
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.goForward()
                    }
                }
                // 刷新 / 停止
                Rectangle {
                    width: 24; height: 24; radius: 6
                    color: reloadMouse.containsMouse ? Theme.hoverFill : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: root.loading() ? "✕" : "⟳"
                        font.pixelSize: 14
                        color: Theme.textPrimary
                    }
                    MouseArea {
                        id: reloadMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.reloadOrStop()
                    }
                }
            }

            // 地址栏
            Rectangle {
                id: addressBox
                anchors.left: parent.left
                anchors.leftMargin: 96
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                height: 24
                radius: 6
                color: shellSettings.darkMode ? Qt.rgba(0, 0, 0, 0.28)
                                              : Qt.rgba(1, 1, 1, 0.85)
                border.width: 1
                border.color: addressInput.activeFocus ? Theme.accent : Theme.separator

                TextInput {
                    id: addressInput
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    selectionColor: Theme.accent
                    selectedTextColor: "#ffffff"
                    clip: true
                    text: root.addressText
                    inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoPredictiveText

                    onAccepted: {
                        const target = normalizeUrl(text)
                        root.addressText = target
                        root.navigate(target)
                        focus = false
                    }
                    // 点进地址栏全选，省得先删一遍
                    onActiveFocusChanged: if (activeFocus) selectAll()

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("搜索或输入网址")
                        font.pixelSize: 11
                        color: Theme.textTertiary
                        visible: addressInput.text === "" && !addressInput.activeFocus
                    }
                }
            }

            // 加载进度条
            Rectangle {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                height: 2
                visible: root.loading()
                color: Theme.accent
                width: parent.width * (root.loadProgress() / 100.0)
            }
        }

        // ——————————————— 设备码提示条
        //
        // 从「登录微软账户」跳过来时才有。把 8 位码顶在页面正上方，
        // 省得用户还要切回启动器去看。
        Rectangle {
            id: codeBar
            width: parent.width
            height: (account.deviceCode !== "" && !account.signedIn) ? 30 : 0
            visible: height > 0
            color: Qt.rgba(0.04, 0.52, 1.0, 0.16)

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("设备码 %1 已复制 —— 粘贴到下面的页面里即可").arg(account.deviceCode)
                    font.pixelSize: 11
                    color: Theme.textPrimary
                }

                Rectangle {
                    width: copyLabel.width + 16
                    height: 19
                    radius: 5
                    color: copyMouse.containsMouse ? Theme.hoverFill : "transparent"
                    border.width: 1
                    border.color: Theme.separator
                    Text {
                        id: copyLabel
                        anchors.centerIn: parent
                        text: qsTr("再复制一次")
                        font.pixelSize: 10
                        color: Theme.textPrimary
                    }
                    MouseArea {
                        id: copyMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: account.copyLastDeviceCode()
                    }
                }
            }
        }

        // ——————————————— 网页
        Item {
            id: contentArea
            width: parent.width
            height: parent.height - tabBar.height - toolbar.height - codeBar.height

            Repeater {
                id: viewRepeater
                model: tabModel

                delegate: WebEngineView {
                    required property int index
                    required property string tabUrl

                    anchors.fill: parent
                    // 非当前标签只是不显示，页面照常留在内存里（切回来不用重载）
                    visible: index === root.currentIndex
                    url: tabUrl
                    backgroundColor: "#ffffff"

                    onUrlChanged: {
                        if (index === root.currentIndex)
                            root.addressText = url
                    }
                    onTitleChanged: {
                        if (title !== "")
                            tabModel.setProperty(index, "tabTitle", title)
                    }
                    onLoadingChanged: function (request) {
                        if (index === root.currentIndex
                                && (request.status === WebEngineView.LoadSucceededStatus
                                    || request.status === WebEngineView.LoadFailedStatus))
                            root.addressText = url
                        if (request.status === WebEngineView.LoadFailedStatus)
                            console.log("页面加载失败:", request.errorString, url)
                    }

                    // ★ 关键：接管新窗口请求。
                    // QtWebEngine 默认对它**不做任何事** —— 用户看到的就是
                    // "点了没反应"。B 站的视频、GitHub 的很多链接都走这条路。
                    // 这里改成在新标签里打开。
                    onNewWindowRequested: function (request) {
                        root.addTab(request.requestedUrl)
                        request.action = WebEngineView.IgnoreRequest
                    }
                }
            }
        }
    }
}
