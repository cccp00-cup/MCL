import QtQuick

// 「账户」窗口：离线账户和微软账户都在这儿操作。
// 从苹果菜单进（和 macOS 一样，账户相关都在那儿）。
Item {
    id: root

    // 设备码（微软登录时露出来给用户抄）
    property string deviceCode: ""
    property string deviceUri: ""
    property string errorText: ""

    Connections {
        target: account
        function onDeviceCodeReady(userCode, verificationUri) {
            root.deviceCode = userCode
            root.deviceUri = verificationUri
            root.errorText = ""
        }
        function onFailed(error) {
            root.errorText = error
            root.deviceCode = ""
        }
        function onChanged() {
            if (!account.busy) {
                root.deviceCode = ""
            }
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
        anchors.margins: 22
        spacing: 14

        // —— 当前身份
        Row {
            spacing: 12

            Rectangle {
                width: 52
                height: 52
                radius: 13
                gradient: Gradient {
                    GradientStop { position: 0.0; color: account.kind === "microsoft" ? "#5aa0ff" : "#8b93a7" }
                    GradientStop { position: 1.0; color: account.kind === "microsoft" ? "#15357c" : "#4a4f5c" }
                }
                Text {
                    anchors.centerIn: parent
                    text: account.signedIn ? account.playerName.charAt(0).toUpperCase() : "?"
                    color: "white"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text {
                    text: account.signedIn ? account.playerName : qsTr("未登录")
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }
                Text {
                    text: account.kind === "microsoft" ? qsTr("微软账户")
                        : (account.kind === "offline" ? qsTr("离线账户") : qsTr("先登录才能启动游戏"))
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
                Text {
                    visible: account.signedIn
                    text: account.uuid
                    font.pixelSize: 10
                    color: Theme.textTertiary
                }
            }
        }

        // —— 状态 / 错误
        Text {
            width: parent.width
            visible: account.busy || root.errorText !== ""
            wrapMode: Text.WordWrap
            text: root.errorText !== "" ? root.errorText : account.status
            font.pixelSize: 12
            color: root.errorText !== "" ? "#c05050" : Theme.textSecondary
        }

        // —— 设备码
        Rectangle {
            width: parent.width
            height: visible ? 168 : 0
            visible: root.deviceCode !== ""
            radius: 9
            color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.04)
            border.width: 1
            border.color: Theme.accent

            Column {
                anchors.centerIn: parent
                spacing: 6
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("设备码已复制到剪贴板 —— 在浏览器里粘贴即可")
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.deviceUri
                    font.pixelSize: 11
                    color: Theme.textTertiary
                }
                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: root.deviceCode
                    font.pixelSize: 24
                    font.family: "monospace"
                    font.weight: Font.DemiBold
                    color: Theme.textPrimary
                }

                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8

                    Repeater {
                        model: [
                            { label: qsTr("重新打开浏览器"), act: "open" },
                            { label: qsTr("复制设备码"), act: "copy" }
                        ]
                        delegate: Rectangle {
                            width: 112
                            height: 26
                            radius: 6
                            color: btnMouse.containsMouse ? Theme.hoverFill : "transparent"
                            border.width: 1
                            border.color: Theme.separator
                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                font.pixelSize: 11
                                color: Theme.textPrimary
                            }
                            MouseArea {
                                id: btnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: modelData.act === "open"
                                           ? account.reopenVerificationPage()
                                           : account.copyLastDeviceCode()
                            }
                        }
                    }
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("输完这个码，这里会自动继续")
                    font.pixelSize: 10
                    color: Theme.textTertiary
                }
            }
        }

        // —— 离线名字
        Column {
            width: parent.width
            spacing: 6
            visible: !account.busy

            Text {
                text: qsTr("离线账户名（最多 16 字符）")
                font.pixelSize: 11
                color: Theme.textSecondary
            }
            Row {
                spacing: 8
                Rectangle {
                    width: parent.parent.width - 100
                    height: 30
                    radius: 7
                    color: shellSettings.darkMode ? Qt.rgba(1, 1, 1, 0.07) : Qt.rgba(0, 0, 0, 0.04)
                    border.width: 1
                    border.color: Theme.separator

                    TextInput {
                        id: offlineName
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.textPrimary
                        font.pixelSize: 13
                        selectByMouse: true
                        clip: true
                        text: account.kind === "offline" ? account.playerName : ""
                        onAccepted: account.signInOffline(text)
                    }
                }
                Rectangle {
                    width: 88
                    height: 30
                    radius: 7
                    color: offlineMouse.containsMouse ? Qt.darker(Theme.accent, 1.1) : Theme.accent
                    Text {
                        anchors.centerIn: parent
                        text: qsTr("用离线身份")
                        font.pixelSize: 12
                        color: "white"
                    }
                    MouseArea {
                        id: offlineMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: account.signInOffline(offlineName.text)
                    }
                }
            }
        }

        // —— 动作按钮
        Row {
            spacing: 8
            visible: !account.busy

            Rectangle {
                width: 130
                height: 30
                radius: 7
                color: msMouse.containsMouse ? Qt.darker("#0a84ff", 1.1) : "#0a84ff"
                Text {
                    anchors.centerIn: parent
                    text: qsTr("登录微软账户")
                    font.pixelSize: 12
                    color: "white"
                }
                MouseArea {
                    id: msMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        root.errorText = ""
                        account.signInMicrosoft()
                    }
                }
            }

            Rectangle {
                width: 88
                height: 30
                radius: 7
                visible: account.signedIn
                color: outMouse.containsMouse ? Theme.hoverFill : "transparent"
                border.width: 1
                border.color: Theme.separator
                Text {
                    anchors.centerIn: parent
                    text: qsTr("退出登录")
                    font.pixelSize: 12
                    color: Theme.textPrimary
                }
                MouseArea {
                    id: outMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: account.signOut()
                }
            }
        }

        Rectangle {
            width: 88
            height: 30
            radius: 7
            visible: account.busy
            color: cancelMouse.containsMouse ? Theme.hoverFill : "transparent"
            border.width: 1
            border.color: Theme.separator
            Text {
                anchors.centerIn: parent
                text: qsTr("取消")
                font.pixelSize: 12
                color: Theme.textPrimary
            }
            MouseArea {
                id: cancelMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: account.cancel()
            }
        }
    }
}
