import QtQuick

// 「控制台」应用：把内核吐出来的日志摊开。
// 内核层只要 emit logLine，这一页就能用；换成 PrismLauncher 内核后，这里会开始
// 出现真实的 java 命令行、下载进度与游戏 stdout。
Item {
    id: root

    property var lines: []

    Rectangle {
        anchors.fill: parent
        color: shellSettings.darkMode ? "#121214" : "#1b1b1f"
    }

    Connections {
        target: kernel
        function onLogLine(line) {
            const next = root.lines.slice()
            next.push(line)
            while (next.length > 500)
                next.shift()
            root.lines = next
        }
        function onLaunchFailed(instanceId, error) {
            const next = root.lines.slice()
            next.push("[mcl] " + instanceId + " 启动失败：" + error)
            root.lines = next
        }
    }

    ListView {
        id: view
        anchors.fill: parent
        anchors.margins: 12
        clip: true
        model: root.lines
        spacing: 2
        onCountChanged: positionViewAtEnd()

        delegate: Text {
            required property string modelData
            width: view.width
            text: modelData
            color: "#c8f0d0"
            font.family: "monospace"
            font.pixelSize: 12
            wrapMode: Text.WrapAnywhere
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.lines.length === 0
        text: qsTr("暂无日志")
        color: "#8a8a92"
        font.pixelSize: 13
    }
}
