import QtQuick
import QtQuick.Controls

// 「系统设置」应用：mcl 的外观。改一下就立刻生效并持久化（ShellSettings 负责存盘）。
Item {
    id: root

    component SettingRow: Item {
        id: row
        property string label: ""
        property string hint: ""
        default property alias content: slot.data

        width: parent.width
        height: 52

        Column {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * 0.42
            spacing: 2
            Text {
                text: row.label
                font.pixelSize: 13
                color: Theme.textPrimary
            }
            Text {
                text: row.hint
                font.pixelSize: 11
                color: Theme.textTertiary
                visible: row.hint !== ""
            }
        }

        Item {
            id: slot
            anchors.left: parent.left
            anchors.leftMargin: parent.width * 0.42
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    component McSlider: Slider {
        id: slider
        from: 2
        to: 64
        live: true

        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.separator

            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }

        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 15
            height: 15
            radius: 7.5
            color: "white"
            border.width: 1
            border.color: Qt.rgba(0, 0, 0, 0.15)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.windowBody
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 18
        contentHeight: column.height
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: 4

            Text {
                text: qsTr("外观")
                font.pixelSize: 14
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                bottomPadding: 6
            }

            SettingRow {
                label: qsTr("深色模式")
                hint: qsTr("菜单栏、Dock 与窗口一起切换")
                Switch {
                    id: darkSwitch
                    checked: shellSettings.darkMode
                    onToggled: shellSettings.darkMode = checked
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            SettingRow {
                label: qsTr("壁纸模糊")
                hint: qsTr("菜单栏与 Dock 的毛玻璃半径")
                McSlider {
                    anchors.fill: parent
                    from: 2
                    to: 64
                    value: shellSettings.blurRadius
                    onMoved: shellSettings.blurRadius = Math.round(value)
                }
            }

            SettingRow {
                label: qsTr("压暗")
                hint: qsTr("毛玻璃整体压暗程度")
                McSlider {
                    anchors.fill: parent
                    from: 0
                    to: 0.6
                    value: shellSettings.dim
                    onMoved: shellSettings.dim = Math.round(value * 100) / 100
                }
            }

            SettingRow {
                label: qsTr("饱和度")
                hint: qsTr("底材的色彩浓度")
                McSlider {
                    anchors.fill: parent
                    from: 0
                    to: 2
                    value: shellSettings.saturation
                    onMoved: shellSettings.saturation = Math.round(value * 100) / 100
                }
            }

            SettingRow {
                label: qsTr("颗粒")
                hint: qsTr("磨砂质感强度")
                McSlider {
                    anchors.fill: parent
                    from: 0
                    to: 1
                    value: shellSettings.grain
                    onMoved: shellSettings.grain = Math.round(value * 100) / 100
                }
            }

            Item {
                width: parent.width
                height: 60

                Column {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: qsTr("桌面壁纸")
                        font.pixelSize: 13
                        color: Theme.textPrimary
                    }
                    Text {
                        text: shellSettings.wallpaperPath === ""
                              ? qsTr("当前：内置壁纸")
                              : shellSettings.wallpaperPath
                        font.pixelSize: 11
                        color: Theme.textTertiary
                        width: root.width * 0.5
                        elide: Text.ElideMiddle
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Rectangle {
                        width: 92
                        height: 26
                        radius: 6
                        color: builtinMouse.containsMouse ? Theme.hoverFill : "transparent"
                        border.width: 1
                        border.color: Theme.separator
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("内置壁纸")
                            font.pixelSize: 12
                            color: Theme.textPrimary
                        }
                        MouseArea {
                            id: builtinMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: shellSettings.useBuiltinWallpaper()
                        }
                    }

                    Rectangle {
                        width: 92
                        height: 26
                        radius: 6
                        color: sysMouse.containsMouse ? Theme.hoverFill : "transparent"
                        border.width: 1
                        border.color: Theme.separator
                        Text {
                            anchors.centerIn: parent
                            text: qsTr("跟随系统")
                            font.pixelSize: 12
                            color: Theme.textPrimary
                        }
                        MouseArea {
                            id: sysMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: shellSettings.useSystemWallpaper()
                        }
                    }
                }
            }
        }
    }
}
