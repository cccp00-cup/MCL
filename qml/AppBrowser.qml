import QtQuick
import QtWebEngine

// 内置浏览器。
//
// 默认搜索引擎是 Bing。地址栏接受两种输入：像网址就当网址打开，
// 否则拿去做搜索 —— 和常见浏览器的行为一致。
//
// 这个文件 import 了 QtWebEngine，所以**只能通过字符串 source 惰性加载**
// （见 McWindow 里的说明）：没有 WebEngine 的环境下，连 Component 声明
// 都会在编译期报 "module QtWebEngine is not installed"。
Item {
    id: root

    readonly property string searchBase: "https://www.bing.com/search?q="
    readonly property string homePage: "https://www.bing.com"

    // 地址栏里的文本（跟随实际加载的地址）
    property string addressText: homePage

    // 把用户输入变成真正要打开的地址
    function normalizeUrl(text) {
        const t = String(text).trim()
        if (t === "")
            return homePage
        // 已经带协议的直接用
        if (/^[a-zA-Z][a-zA-Z0-9+.\-]*:\/\//.test(t))
            return t
        // 像域名（有点、没空格）就补 https
        if (!/\s/.test(t) && /^[^\s/]+\.[^\s/]{2,}/.test(t))
            return "https://" + t
        // 其余当搜索词，交给 Bing
        return searchBase + encodeURIComponent(t)
    }

    // 外部（比如"用浏览器打开登录页"）可以直接调它
    function open(url) {
        view.url = url
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Column {
        anchors.fill: parent
        spacing: 0

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
                    color: backMouse.containsMouse && view.canGoBack
                           ? Theme.hoverFill : "transparent"
                    opacity: view.canGoBack ? 1.0 : 0.35
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
                        enabled: view.canGoBack
                        cursorShape: Qt.PointingHandCursor
                        onClicked: view.goBack()
                    }
                }
                // 前进
                Rectangle {
                    width: 24; height: 24; radius: 6
                    color: fwdMouse.containsMouse && view.canGoForward
                           ? Theme.hoverFill : "transparent"
                    opacity: view.canGoForward ? 1.0 : 0.35
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
                        enabled: view.canGoForward
                        cursorShape: Qt.PointingHandCursor
                        onClicked: view.goForward()
                    }
                }
                // 刷新 / 停止
                Rectangle {
                    width: 24; height: 24; radius: 6
                    color: reloadMouse.containsMouse ? Theme.hoverFill : "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: view.loading ? "✕" : "⟳"
                        font.pixelSize: 14
                        color: Theme.textPrimary
                    }
                    MouseArea {
                        id: reloadMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: view.loading ? view.stop() : view.reload()
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
                        root.addressText = normalizeUrl(text)
                        view.url = root.addressText
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
                visible: view.loading
                color: Theme.accent
                width: parent.width * (view.loadProgress / 100.0)
            }
        }

        // ——————————————— 网页
        WebEngineView {
            id: view
            width: parent.width
            height: parent.height - toolbar.height
            url: root.homePage
            backgroundColor: "#ffffff"

            // 地址栏跟着实际地址走（后退/点链接也会更新）
            onUrlChanged: root.addressText = url

            onLoadingChanged: function (request) {
                if (request.status === WebEngineView.LoadSucceededStatus
                        || request.status === WebEngineView.LoadFailedStatus)
                    root.addressText = url
                if (request.status === WebEngineView.LoadFailedStatus)
                    console.log("页面加载失败:", request.errorString, url)
            }
        }
    }
}
