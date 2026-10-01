import QtQuick

// 「关于本机」应用。
Item {
    id: root

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 26
        spacing: 6

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 74
            height: 74
            radius: 17
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#5aa0ff" }
                GradientStop { position: 1.0; color: "#15357c" }
            }

            Image {
                anchors.centerIn: parent
                width: 46
                height: 46
                source: "qrc:/mcl/icons/cube.svg"
                sourceSize: Qt.size(46, 46)
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "mcl"
            font.pixelSize: 20
            font.weight: Font.DemiBold
            color: Theme.textPrimary
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("版本 %1").arg(Qt.application.version)
            font.pixelSize: 11
            color: Theme.textSecondary
        }
    }

    Column {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 18
        width: Math.min(parent.width - 60, 420)
        spacing: 5

        Repeater {
            model: [
                { k: qsTr("启动内核"), v: kernel.name + (kernel.available ? "" : qsTr("（未接入）")) },
                { k: qsTr("平台"), v: Qt.platform.os },
                { k: qsTr("界面"), v: qsTr("Qt 6 · QML 全自绘") }
            ]

            delegate: Row {
                required property var modelData
                width: parent.width
                spacing: 10

                Text {
                    width: 92
                    text: modelData.k
                    horizontalAlignment: Text.AlignRight
                    font.pixelSize: 11
                    color: Theme.textSecondary
                }
                Text {
                    width: parent.width - 102
                    text: modelData.v
                    font.pixelSize: 11
                    color: Theme.textPrimary
                    elide: Text.ElideMiddle
                }
            }
        }
    }
}
